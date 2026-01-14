#include "pipeline.hh"

#include <functional>

#include "glib.h"
namespace vptyp {
gboolean Pipeline::bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
  GMainLoop *loop = (GMainLoop *)data;

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
      g_print("End of stream\n");
      g_main_loop_quit(loop);
      break;

    case GST_MESSAGE_ERROR: {
      gchar *debug;
      GError *error;

      gst_message_parse_error(msg, &error, &debug);
      g_free(debug);

      g_printerr("Error: %s\n", error->message);
      g_error_free(error);

      g_main_loop_quit(loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

void Pipeline::play() {
  gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
}

void Pipeline::stop() { gst_element_set_state(pipeline.get(), GST_STATE_NULL); }

Pipeline::Pipeline(GMainLoop &loop, std::string_view name) : loop(loop) {
  using namespace std::placeholders;
  pipeline = make_gst(gst_pipeline_new(name.data()));

  auto gstBus = make_gst(gst_pipeline_get_bus(GST_PIPELINE(pipeline.get())));

  bus_watch_id = gst_bus_add_watch(gstBus.get(), &Pipeline::bus_call, &loop);
}

void Pipeline::add_element(Element &&element) {
  gst_bin_add(reinterpret_cast<GstBin *>(pipeline.get()),
              element.element.get());
  element.owned = 1;
}

void Pipeline::add_element(Element &element) {
  gst_bin_add(reinterpret_cast<GstBin *>(pipeline.get()),
              element.element.get());
  element.owned = 1;
}

}  // namespace vptyp