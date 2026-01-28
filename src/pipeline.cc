#include "pipeline.hh"

#include <glog/logging.h>

#include <format>

#include "glib.h"
#include "gst/gstmessage.h"
namespace vptyp {

gboolean Pipeline::bus_handler(GstBus* bus, GstMessage* msg) {
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS: {
      g_print("End of stream\n");
      g_main_loop_quit(&loop);
      return false;
    }
    case GST_MESSAGE_ERROR: {
      gchar* debug;
      GError* error;

      gst_message_parse_error(msg, &error, &debug);

      LOG(ERROR) << std::format("Error: {}; Debug: {}", error->message,
                                debug ? debug : "none");
      g_free(debug);
      g_error_free(error);

      g_main_loop_quit(&loop);
      return false;
    }
    case GST_MESSAGE_INFO: {
      GError* err;
      gchar* debug;
      gst_message_parse_info(msg, &err, &debug);
      LOG(INFO) << std::format("err: {}, debug: {}", err->message, debug);

      g_free(debug);
      g_error_free(err);
      break;
    }
    default:
      auto type_name = gst_message_type_get_name((GST_MESSAGE_TYPE(msg)));
      LOG(INFO) << std::format("Received message of type {}", type_name);
      break;
  }
  return true;
}

gboolean Pipeline::bus_call(GstBus* bus, GstMessage* msg, gpointer data) {
  auto that = static_cast<Pipeline*>(data);
  return that->bus_handler(bus, msg);
}

void Pipeline::play() {
  gst_element_set_state(pipeline.get(), GST_STATE_PLAYING);
}

void Pipeline::stop() { gst_element_set_state(pipeline.get(), GST_STATE_NULL); }

Pipeline::Pipeline(GMainLoop& loop, std::string_view name) : loop(loop) {
  using namespace std::placeholders;
  pipeline = make_gst(gst_pipeline_new(name.data()));

  auto gstBus = make_gst(gst_pipeline_get_bus(GST_PIPELINE(pipeline.get())));

  bus_watch_id = gst_bus_add_watch(gstBus.get(), bus_call, this);
}

Pipeline::~Pipeline() {
  gst_element_set_state(pipeline.get(), GST_STATE_NULL);
  if (bus_watch_id != 0) {
    g_source_remove(bus_watch_id);
    bus_watch_id = 0;
  }
  elements.clear();
}

void Pipeline::add_element_internal(Element& element) {
  gst_object_ref(element.element.get());
  gst_bin_add(reinterpret_cast<GstBin*>(pipeline.get()), element.element.get());
}

void Pipeline::add_element(Element&& element) {
  add_element_internal(element);
  elements.push_back(std::make_unique<Element>(std::move(element)));
}

void Pipeline::add_elements(std::list<Element>&& elements) {
  for (auto& element : elements) {
    add_element_internal(element);
    this->elements.push_back(std::make_unique<Element>(std::move(element)));
  }
}

Element& Pipeline::get_element(std::string_view alias) {
  for (auto& element : elements) {
    if (element->alias == alias) {
      return *element;
    }
  }
  throw std::runtime_error("Element with alias not found");
}

}  // namespace vptyp
