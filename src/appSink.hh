#pragma once
#include <thread>

#include "element.hh"
#include "gst/gstpad.h"
namespace vptyp {

class AppSink : public Element {
 public:
  AppSink(std::string_view alias);  // factory
  AppSink(AppSink&& other);
  AppSink& operator=(AppSink&& other);
  AppSink(AppSink& other) = delete;
  AppSink& operator=(AppSink&) = delete;
  ~AppSink() override;

  using SampleCallback = std::function<void(GstSample* sample)>;
  void set_sample_callback(SampleCallback callback);
  void thread_run();

 protected:
  GstFlowReturn process_sample();
  void handle_sample_callback();
  void reattach_sample_callback();

 protected:
  std::jthread thr;
  SampleCallback sampleCallback{nullptr};
  gulong sampleCallbackId{0};
};

}  // namespace vptyp