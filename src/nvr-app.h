#include <gst/gst.h>


typedef struct _Options {
  gchar   *src;
  gchar   *dst;
  guint64 nsecs;
} Options;

int start_nvr(const Options *opts);
