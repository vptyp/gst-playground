#include "appSink.hh"

#include <gtest/gtest.h>

#include <element.hh>
#include <future>
#include <pipeline.hh>

#include "glib.h"
#include "logger.hh"

class AppSinkTest : public ::testing::Test {
 public:
  void playPipelineAndWait(vptyp::Pipeline& pipeline) {
    pipeline.play();
    auto waiter = std::async(std::launch::async, [this]() {
      g_main_loop_run(loop);
      return true;
    });

    std::future_status status =
        waiter.wait_for(std::chrono::milliseconds(1000));
    if (status == std::future_status::timeout) {
      g_main_loop_quit(loop);
    }
    pipeline.stop();
    EXPECT_NE(status, std::future_status::timeout);
  }

 protected:
  void SetUp() override {
    loggerSetup(nullptr);
    gst_init(nullptr, nullptr);
    loop = g_main_loop_new(nullptr, false);
  }
  void TearDown() override {
    g_main_loop_unref(loop);
    loop = nullptr;
  }

 public:
  GMainLoop* loop{nullptr};
};

TEST_F(AppSinkTest, Init) {
  EXPECT_NO_THROW(vptyp::AppSink sink("base"));
  vptyp::AppSink test("test");
  vptyp::AppSink movedTest(std::move(test));
  EXPECT_EQ(test.is_initialised(), false);
  EXPECT_EQ(movedTest.is_initialised(), true);
}

TEST_F(AppSinkTest, GetCallback) {
  using namespace vptyp;
  Pipeline pipeline(*loop, "test");
  Element src("videotestsrc", "src");
  AppSink sink("sink");
  sink.object_set("emit-signals", true, "sync", false);

  AppSink::SampleCallback sinkCallback = [this](GstSample* sample) {
    LOG(INFO) << std::format("Sample callback fired");
    EXPECT_NE(sample, nullptr);
    if (sample) {
      LOG(INFO) << std::format("Got something in sample");
      GstCaps* caps = gst_sample_get_caps(sample);
      auto caps_str = gst_caps_to_string(caps);
      LOG(INFO) << std::format("associated caps is {} ; ", caps_str);
      g_free(caps_str);
    }

    g_main_loop_quit(loop);
  };

  sink.set_sample_callback(std::move(sinkCallback));

  pipeline.add_element(std::move(src));
  pipeline.add_element(std::move(sink));
  pipeline.get_element("src").link(pipeline.get_element("sink"));

  playPipelineAndWait(pipeline);
}