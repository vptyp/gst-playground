#include "element_test.hh"

#include <gtest/gtest.h>

#include <element.hh>
#include <future>
#include <pipeline.hh>

#include "glib.h"
#include "logger.hh"

namespace test {

bool Element::on_pad_added(GstElement* src, GstPad* new_pad,
                           GstElement* target) {
  LOG(INFO) << std::format("{}:{}", __func__, "called");
  EXPECT_TRUE(vptyp::Element::on_pad_added(src, new_pad, target));
  g_main_loop_quit(loop);
  return true;
}

GstElement* Element::get() { return vptyp::Element::element.get(); }

Element::Element(GMainLoop* loop, std::string_view element_name,
                 std::string_view alias)
    : vptyp::Element(element_name, alias), loop(loop) {}
}  // namespace test

class ElementTest : public ::testing::Test {
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

TEST_F(ElementTest, ValidElementCreation) {
  loggerSetup(nullptr);
  vptyp::Element element("videotestsrc", "test-src");
  EXPECT_TRUE(element.is_initialised());
}

TEST_F(ElementTest, InvalidElementCreation) {
  vptyp::Element element("nonexistent-element", "test-src");
  EXPECT_FALSE(element.is_initialised());
}

TEST_F(ElementTest, ElementLinking) {
  vptyp::Element src("videotestsrc", "src");
  vptyp::Element sink("autovideosink", "sink");

  EXPECT_TRUE(src.is_initialised());
  EXPECT_TRUE(sink.is_initialised());
  EXPECT_TRUE(src.link(sink)) << "Failed to link videotestsrc to autovideosink";
}

TEST_F(ElementTest, ElementPropertySetting) {
  vptyp::Element src("videotestsrc", "src");
  EXPECT_TRUE(src.is_initialised());

  src.object_set("is-live", TRUE);
  EXPECT_TRUE(src.object_get<bool>("is-live"));
}

TEST_F(ElementTest, ElementMoveSemantics) {
  vptyp::Element original("videotestsrc", "original");
  EXPECT_TRUE(original.is_initialised());

  vptyp::Element moved = std::move(original);
  EXPECT_FALSE(original.is_initialised());
  EXPECT_TRUE(moved.is_initialised());
}

TEST_F(ElementTest, DynamicPadElement) {
  vptyp::Pipeline pipeline(*loop, "heh");
  vptyp::Element src("videotestsrc", "testsrc");
  vptyp::Element decodebin("decodebin", "decodebin");
  EXPECT_TRUE(decodebin.is_initialised());

  // decodebin has SOMETIMES pads, which should be handled correctly
  vptyp::Element sink("fakesink", "testsink");
  EXPECT_TRUE(sink.is_initialised());

  sink.set_caps_callback([this](GstCaps* caps) { g_main_loop_quit(loop); });

  pipeline.add_element(std::move(src));
  pipeline.add_element(std::move(decodebin));
  pipeline.add_element(std::move(sink));

  pipeline.get_element("testsrc").link(pipeline.get_element("decodebin"));
  pipeline.get_element("decodebin").link(pipeline.get_element("testsink"));

  pipeline.play();
  auto waiter = std::async(std::launch::async, [this]() {
    g_main_loop_run(loop);
    return true;
  });

  std::future_status status = waiter.wait_for(std::chrono::milliseconds(1000));
  if (status == std::future_status::timeout) {
    g_main_loop_quit(loop);
  }
  pipeline.stop();

  EXPECT_NE(status, std::future_status::timeout);
}

TEST_F(ElementTest, DynamicPadElementMove) {
  vptyp::Pipeline pipeline(*loop, "pipeline");

  vptyp::Element sink("fakesink", "testsink");
  sink.set_caps_callback([this](GstCaps*) { g_main_loop_quit(loop); });

  std::list<vptyp::Element> elements;
  elements.push_back(vptyp::Element("videotestsrc", "testsrc"));
  elements.push_back(vptyp::Element("decodebin", "decodebin"));
  elements.push_back(std::move(sink));

  pipeline.add_elements(std::move(elements));
  pipeline.get_element("testsrc").link(pipeline.get_element("decodebin"));
  pipeline.get_element("decodebin").link(pipeline.get_element("testsink"));

  pipeline.play();

  auto waiter = std::async(std::launch::async, [this]() {
    g_main_loop_run(loop);
    return true;
  });

  std::future_status status = waiter.wait_for(std::chrono::milliseconds(1000));
  if (status == std::future_status::timeout) {
    g_main_loop_quit(loop);
  }
  pipeline.stop();

  EXPECT_NE(status, std::future_status::timeout);
}

TEST_F(ElementTest, MultipleElementLinking) {
  vptyp::Element src("videotestsrc", "src");
  vptyp::Element convert("videoconvert", "convert");
  vptyp::Element sink("autovideosink", "sink");

  EXPECT_TRUE(src.is_initialised());
  EXPECT_TRUE(convert.is_initialised());
  EXPECT_TRUE(sink.is_initialised());

  std::list<std::unique_ptr<vptyp::Element>> elements;
  elements.push_back(std::make_unique<vptyp::Element>(std::move(convert)));
  elements.push_back(std::make_unique<vptyp::Element>(std::move(sink)));

  EXPECT_TRUE(src.link(elements.begin(), elements.end()))
      << "Failed to link chain of elements";
}

TEST_F(ElementTest, CapsCallback) {
  vptyp::Pipeline pipeline(*loop, "pipeline");
  vptyp::Element src("videotestsrc", "src");
  vptyp::Element convert("videoconvert", "convert");
  vptyp::Element sink("fakesink", "sink");

  sink.object_set("sync", FALSE);

  bool callback_called = false;
  sink.set_caps_callback([&callback_called, this](GstCaps* caps) {
    callback_called = true;
    char* caps_str = gst_caps_to_string(caps);
    LOG(INFO) << caps_str;
    g_free(caps_str);
    g_main_loop_quit(loop);
  });

  pipeline.add_element(std::move(src));
  pipeline.add_element(std::move(convert));
  pipeline.add_element(std::move(sink));

  ASSERT_TRUE(
      pipeline.get_element("src").link(pipeline.get_element("convert")));
  ASSERT_TRUE(
      pipeline.get_element("convert").link(pipeline.get_element("sink")));

  pipeline.play();

  // Run loop with timeout
  auto waiter =
      std::async(std::launch::async, [this]() { g_main_loop_run(loop); });

  if (waiter.wait_for(std::chrono::seconds(2)) == std::future_status::timeout) {
    g_main_loop_quit(loop);
  }

  pipeline.stop();
  EXPECT_TRUE(callback_called);
}

TEST_F(ElementTest, CapsCallbackMove) {
  vptyp::Pipeline pipeline(*loop, "pipeline");
  vptyp::Element src("videotestsrc", "src");
  vptyp::Element convert("videoconvert", "convert");
  vptyp::Element sink("fakesink", "sink");

  sink.object_set("sync", FALSE);

  bool callback_called = false;
  sink.set_caps_callback([&callback_called, this](GstCaps* caps) {
    callback_called = true;
    g_main_loop_quit(loop);
  });

  // Move sink
  vptyp::Element moved_sink = std::move(sink);

  pipeline.add_element(std::move(src));
  pipeline.add_element(std::move(convert));
  pipeline.add_element(std::move(moved_sink));

  ASSERT_TRUE(
      pipeline.get_element("src").link(pipeline.get_element("convert")));
  ASSERT_TRUE(
      pipeline.get_element("convert").link(pipeline.get_element("sink")));

  pipeline.play();

  auto waiter =
      std::async(std::launch::async, [this]() { g_main_loop_run(loop); });

  if (waiter.wait_for(std::chrono::seconds(2)) == std::future_status::timeout) {
    g_main_loop_quit(loop);
  }

  pipeline.stop();
  EXPECT_TRUE(callback_called);
}
