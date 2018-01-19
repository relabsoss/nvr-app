#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "nvr-app.h"

#define TO_NSECS(t) ((guint64)t * (guint64)1000000000)

int
main (int argc, char *argv[])
{
  Options opts = {NULL, NULL, TO_NSECS(5) };
  guint64 secs = 5;
  int ret = 0;

  const GOptionEntry entries[] = {
    /* you can add your won command line options here */
    { "src", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &(opts.src),
      "Stream source", NULL },
    { "dst", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &(opts.dst),
      "Folder to save video files", NULL },
    { "secs", 0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT64, &secs,
      "Seconds to seek back before starting recording", NULL },
    { NULL, }
  };
  GOptionContext *ctx;
  GError *err = NULL;

  ctx = g_option_context_new ("--src [RTSP URI] --dst [FOLDER PATH] --secs [SEEK TIME]");
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  g_option_context_add_main_entries (ctx, entries, NULL);

  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    return -1;
  }

  g_option_context_free (ctx);

  opts.nsecs = TO_NSECS(secs);
  g_print("NSecs %lu\n", opts.nsecs);

  ret = start_nvr(&opts);

  g_free (opts.src);
  g_free (opts.dst);

  return ret;
}
