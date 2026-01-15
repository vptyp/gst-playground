#include <gtest/gtest.h>

#include <element.hh>
#include <pipeline.hh>
#include <string>

static constexpr size_t test_timeout = 3;

TEST(SalutationTest, Static) {
  EXPECT_EQ(std::string("Hello World!"), std::string("Hello World!"));
}

TEST(MinPipeline, VideoTestSrc) {
  GMainLoop* loop;
  gst_init(nullptr, nullptr);
  loop = g_main_loop_new(NULL, FALSE);

  auto pipeline = vptyp::Pipeline(*loop, "base");
  auto src = vptyp::Element("videotestsrc", "src");
  auto sink = vptyp::Element("autovideosink", "sink");
  pipeline.add_element(src);
  pipeline.add_element(sink);
  EXPECT_EQ(src.link(sink), true);
  pipeline.play();
  sleep(test_timeout);
  pipeline.stop();
}

TEST(CrashTest, NoElement) {
  GMainLoop* loop;
  gst_init(nullptr, nullptr);
  loop = g_main_loop_new(NULL, FALSE);
  auto pipeline = vptyp::Pipeline(*loop, "base");
  EXPECT_NO_FATAL_FAILURE(auto src = vptyp::Element("Crash", "src"));
}

int main() {
  testing::InitGoogleTest();
  return RUN_ALL_TESTS();
}