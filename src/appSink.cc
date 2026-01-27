#include "appSink.hh"

#include <glog/logging.h>
#include <gst/app/gstappsink.h>

#include <thread>

#include "glib-object.h"
#include "gst/gstpad.h"
#include "src/element.hh"
namespace vptyp {

AppSink::AppSink(std::string_view alias) : Element("appsink", alias) {
  handle_sample_callback();
}
AppSink::AppSink(AppSink&& other) : Element(std::move(other)) {
  std::swap(sampleCallbackId, other.sampleCallbackId);
  std::swap(sampleCallback, other.sampleCallback);
  reattach_sample_callback();
}
AppSink& AppSink::operator=(AppSink&& other) {
  this->Element::operator=(std::move(other));
  std::swap(sampleCallbackId, other.sampleCallbackId);
  std::swap(sampleCallback, other.sampleCallback);
  other.sampleCallbackId = 0;
  other.sampleCallback = nullptr;

  reattach_sample_callback();
  return *this;
}

AppSink::~AppSink() {
  if (sampleCallbackId > 0)
    g_signal_handler_disconnect(element.get(), sampleCallbackId);
}

void AppSink::set_sample_callback(SampleCallback callback) {
  sampleCallback = std::move(callback);
}

void AppSink::thread_run() {
  thr = std::jthread([this]() {
    while (true) {
      process_sample();
    }
  });
}

void AppSink::handle_sample_callback() {
  auto elementLambda = [this](GstElement* src, gpointer data) {
    return this->process_sample();
  };
  using LambdaType = decltype(elementLambda);
  auto* lambdaPtr = new LambdaType(std::move(elementLambda));

  sampleCallbackId = g_signal_connect_data(
      this->element.get(), "new-sample",
      reinterpret_cast<void (*)()>(
          +[](GstElement* src, gpointer data) -> GstFlowReturn {
            auto* func = static_cast<LambdaType*>(data);
            return (*func)(src, nullptr);
          }),
      lambdaPtr,
      +[](gpointer data, GClosure* closure) {
        delete static_cast<LambdaType*>(data);
      },
      GConnectFlags(0));
}

void AppSink::reattach_sample_callback() {
  if (sampleCallbackId == 0 || !element) return;

  g_signal_handler_disconnect(element.get(), sampleCallbackId);

  handle_sample_callback();
}

GstFlowReturn AppSink::process_sample() {
  auto sample = make_gst<GstSample>(
      gst_app_sink_pull_sample(GST_APP_SINK(element.get())));
  GstFlowReturn ret = GstFlowReturn::GST_FLOW_OK;
  if (sampleCallback) sampleCallback(sample.get());

  return ret;
}

}  // namespace vptyp