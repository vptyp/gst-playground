#include "webPlayer.hh"

#include <gst/gst.h>

#include "element.hh"

namespace vptyp {
WebToFilePlayer::WebToFilePlayer(GMainLoop& loop, std::string_view url,
                                 std::string_view output_file)
    : BasePlayer(),
      url(url),
      output_file(output_file),
      loop(loop),
      pipeline(loop, "web-to-file-player") {}

void WebToFilePlayer::play() { pipeline.play(); }
void WebToFilePlayer::stop() { pipeline.stop(); }

void WebToFilePlayer::create() {
  Element src = Element("souphttpsrc", "web-source");
  src.object_set("location", url.data());

  auto demux = Element("qtdemux", "demuxer");
  auto parse = Element("h264parse", "h264parse");
  auto decoder = Element("avdec_h264", "decoder");
  auto converter = Element("videoconvert", "converter");
  auto encoder = Element("x264enc", "encoder");
  auto muxer = Element("mp4mux", "muxer");

  Element sink = Element("filesink", "file-sink");
  sink.object_set("location", output_file.data());

  std::list<Element> elements;

  elements.push_back(std::move(demux));
  elements.push_back(std::move(parse));
  elements.push_back(std::move(decoder));
  elements.push_back(std::move(converter));
  elements.push_back(std::move(encoder));
  elements.push_back(std::move(muxer));
  elements.push_back(std::move(sink));

  pipeline.add_element(std::move(src));
  pipeline.add_elements(std::move(elements));

  auto& owned = pipeline.get_elements();
  pipeline.get_element("web-source").link(++owned.begin(), owned.end());
}

}  // namespace vptyp