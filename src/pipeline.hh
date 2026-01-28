#pragma once
#include <gst/gst.h>

#include <list>
#include <memory>
#include <string_view>
#include <type_traits>

#include "element.hh"
#include "glib.h"

namespace vptyp {

class Pipeline {
 public:
  Pipeline(GMainLoop& loop, std::string_view name);
  ~Pipeline();

  void add_element(Element&& element);

  template <typename T>
  void add_element(T&& element)
    requires(!std::is_same_v<std::decay_t<T>, Element>)
  {
    static_assert(std::is_base_of_v<Element, std::decay_t<T>>,
                  "T must be an Element");
    add_element_internal(element);
    elements.push_back(
        std::make_unique<std::decay_t<T>>(std::forward<T>(element)));
  }

  void add_elements(std::list<Element>&& elements);

  void play();
  void stop();

  std::list<std::unique_ptr<Element>>& get_elements() { return elements; }

  Element& get_element(std::string_view alias);

 protected:
  static gboolean bus_call(GstBus* bus, GstMessage* msg, gpointer data);
  gboolean bus_handler(GstBus* bus, GstMessage* msg);
  void add_element_internal(Element& element);

 protected:
  GMainLoop& loop;
  guint bus_watch_id;
  std::unique_ptr<GstElement, Deleter<GstElement>> pipeline{nullptr};
  std::list<std::unique_ptr<Element>> elements;  // owned elements
};

}  // namespace vptyp