#include <gst/gst.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib/gprintf.h>

#define DEFAULT_MIN_THRESHOLD_TIME 5000000000

typedef struct _Context {
  GMainLoop  *loop;
  const gchar *src;
  const gchar *dst;
  GstElement *pipeline;
  GstElement *source;
  GstElement *queue;
  GstElement *valve;
  GstElement *sink_source;
  GstElement *sink;
} Context;

static void
on_rtsp_pad_added (GstElement *element,
              GstPad     *pad,
              gpointer    data)
{
  GstPad *sinkpad;
  Context *context = (Context *) data;
  GstElement *source = context->source;
  GMainLoop *loop = context->loop;
  GstPadLinkReturn ret;

  g_print ("Dynamic pad created, linking source and queue.\n");

  sinkpad = gst_element_get_static_pad (source, "sink");

  ret = gst_pad_link (pad, sinkpad);

  if (ret != GST_PAD_LINK_OK) {
    g_printerr ("Linking pads error: %d\n", ret);
    g_main_loop_quit (loop);
  }

  gst_object_unref (sinkpad);
}


static gboolean
bus_call (GstBus     *bus,
          GstMessage *msg,
          gpointer    data)
{
  GMainLoop *loop = (GMainLoop *) data;

  switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS:
      g_print ("End of stream\n");
      g_main_loop_quit (loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar  *debug;
      GError *error;

      gst_message_parse_error (msg, &error, &debug);
      g_free (debug);

      g_printerr ("Error: %s\n", error->message);
      g_error_free (error);

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

gboolean
on_start_recording(gpointer user_data) {
  g_print ("Start recording handler called\n");

  Context *context = (Context *) user_data;
  GstElement *pipeline = context->pipeline;
  GstElement *queue = context->queue;
  GstElement *valve = context->valve;
  GstElement *sink_source = context->sink_source;
  GstElement *sink = context->sink = gst_element_factory_make ("filesink", NULL);

  GDateTime *dt = g_date_time_new_now_local();

  g_object_set (sink, "location", g_date_time_format(dt, "file_%d_%m_%Y_%H_M_%S.mp4"), NULL);
  if (gst_bin_add (GST_BIN( pipeline ), sink) != TRUE) {
    g_printerr ("Sink not added to pipeline.\n");
    return G_SOURCE_CONTINUE;
  }
  if (gst_element_link (sink_source, sink) != TRUE) {
    g_printerr ("Sink not linked.\n");
    return G_SOURCE_CONTINUE;
  }

  gst_element_set_state(sink, GST_STATE_PLAYING);

  g_object_set (valve, "drop", FALSE, NULL);
  g_object_set (queue, "min-threshold-time", 0, NULL);

  g_date_time_unref(dt);

  return G_SOURCE_CONTINUE;
}

gboolean
on_stop_recording(gpointer user_data) {
  g_print ("Stop recording handler called\n");

  Context *context = (Context *) user_data;
  GstElement *pipeline = context->pipeline;
  GstElement *queue = context->queue;
  GstElement *valve = context->valve;
  GstElement *sink_source = context->sink_source;
  GstElement *sink  = context->sink;

  gst_object_ref(sink);
  gst_element_unlink(sink_source, sink);
  gst_element_set_state(sink, GST_STATE_NULL);
  if (gst_bin_remove (GST_BIN( pipeline ), sink) != TRUE) {
    g_printerr ("Sink not removed from pipeline.\n");
    return G_SOURCE_CONTINUE;
  }
  gst_object_unref(sink);

  context->sink = 0;

  g_object_set (queue, "min-threshold-time", DEFAULT_MIN_THRESHOLD_TIME, NULL);
  g_object_set (valve, "drop", TRUE, NULL);

  return G_SOURCE_CONTINUE;
}

gboolean
on_stop(gpointer user_data) {
  g_print ("Stop signal handler called\n");

  GMainLoop *loop = ((Context *) user_data)->loop;
  g_main_loop_quit (loop);

  return G_SOURCE_CONTINUE;
}

void
init_signal_handler(Context *context) {
  g_unix_signal_add(SIGUSR1, on_start_recording, context);
  g_unix_signal_add(SIGUSR2, on_stop_recording,  context);
  g_unix_signal_add(SIGINT, on_stop,  context);
  g_unix_signal_add(SIGTERM, on_stop,  context);
}

int start_nvr(const char* src, const char* dst) {
  Context context;
  context.src = src;
  context.dst = dst;

  GMainLoop  *loop;

  GstElement *pipeline;
  GstElement *source;
  GstElement *queue, *valve, *rtph264depay, *mpegtsmux;

  GstBus *bus;
  GstStateChangeReturn ret;
  guint bus_watch_id;

  /* Initialize GStreamer */
  gst_init (NULL, NULL);

  loop = context.loop = g_main_loop_new (NULL, FALSE);
  init_signal_handler(&context);

  g_print("PID: %d\n", getpid());

  /* Create the elements */
  source = gst_element_factory_make ("rtspsrc", NULL);

  queue = context.queue = gst_element_factory_make("queue", NULL);
  valve = context.valve = gst_element_factory_make("valve", NULL);
  rtph264depay = context.source = gst_element_factory_make ("rtph264depay", NULL);
  mpegtsmux = context.sink_source = gst_element_factory_make ("mpegtsmux", NULL);

  /* Create the empty pipeline */
  pipeline = context.pipeline = gst_pipeline_new ("rtsp-pipeline");

  if (!pipeline || !source || !queue || !valve || !rtph264depay || !mpegtsmux) {
    g_printerr ("Not all elements could be created.\n");
    return 1;
  }

  /* Modify the source's properties */
  g_print("RTSP location is %s\n", src);
  g_object_set (source, "location", src, NULL);
  g_object_set (queue, "max-size-buffers", 0, NULL);
  g_object_set (queue, "max-size-time", 0, NULL);
  g_object_set (queue, "max-size-bytes", 0, NULL);
  g_object_set (queue, "min-threshold-time", DEFAULT_MIN_THRESHOLD_TIME, NULL);
  g_object_set (valve, "drop", TRUE, NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);

  /* Build the pipeline */
  gst_bin_add_many (GST_BIN (pipeline), source, rtph264depay, queue, valve, mpegtsmux, NULL);
  g_signal_connect (source, "pad-added", G_CALLBACK (on_rtsp_pad_added), &context);

  if (gst_element_link_many (rtph264depay, queue, valve, mpegtsmux, NULL) != TRUE) {
    g_printerr ("Elements could not be linked.\n");
    gst_object_unref (pipeline);
    return 2;
  }

  /* Start playing */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_printerr ("Unable to set the pipeline to the playing state.\n");
    gst_object_unref (pipeline);
    return 3;
  }

  /* Wait until error or EOS */
  g_main_loop_run (loop);

  /* Free resources */
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
  return 0;
}