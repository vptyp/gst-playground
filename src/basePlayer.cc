#include "basePlayer.hh"

#include <glog/logging.h>
#include <gst/gst.h>

#include <algorithm>
#include <format>

#include "element.hh"
namespace vptyp {
VideoPlayback::VideoPlayback(GMainLoop& loop, std::string_view file)
    : BasePlayer(), file(file), loop(loop), pipeline(loop, "video-playback") {}

void VideoPlayback::play() { pipeline.play(); }
void VideoPlayback::stop() { pipeline.stop(); }

void VideoPlayback::create() {
  LOG(INFO) << std::format("location: {}", file.data());

  Element filesrc = Element("filesrc", "filesrc");
  filesrc.object_set("location", file.c_str());
  auto decodeBin = Element("decodebin", "decodebin");
  auto converter = Element("videoconvert", "converter");
  auto videosink = Element("autovideosink", "video-output");

  std::list<Element> elements;
  elements.push_back(std::move(filesrc));
  elements.push_back(std::move(decodeBin));
  elements.push_back(std::move(converter));
  elements.push_back(std::move(videosink));
  std::ranges::for_each(elements, [this](Element& element) {
    pipeline.add_element(std::move(element));
  });
  auto& owned = pipeline.get_elements();
  auto res = (*owned.begin())->link(++owned.begin(), owned.end());

  if (!res) {
    LOG(ERROR) << std::format("Linkage failed");
  }
}

}  // namespace vptyp