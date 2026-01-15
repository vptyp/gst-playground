#include <glog/logging.h>
#include <gst/gst.h>

static void on_pad_added(GstElement* src, GstPad* new_pad, gpointer data) {
  GstElement* target = GST_ELEMENT(data);
  GstPad* sink_pad = gst_element_get_static_pad(target, "sink");

  // Check if the sink is already linked (e.g., if multiple pads are found)
  if (gst_pad_is_linked(sink_pad)) {
    g_object_unref(sink_pad);
    return;
  }

  // Check if the new pad's caps are compatible with the target
  GstPadLinkReturn ret = gst_pad_link(new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED(ret)) {
    LOG(ERROR) << "Failed to link dynamic pad to "
               << gst_element_get_name(target);
  } else {
    LOG(INFO) << "Successfully linked dynamic pad to "
              << gst_element_get_name(target);
  }

  g_object_unref(sink_pad);
}