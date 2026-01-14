#pragma once
#include <gst/gst.h>

#include <list>
#include <memory>
#include <string_view>

#include "gst/gstelement.h"
#include "gstDeleter.hh"
namespace vptyp {

class Pipeline;

class Element {
 public:
  Element(std::string_view element_name, std::string_view alias);  // factory
  Element(Element&& other);
  virtual ~Element();

  bool is_initialised();
  bool is_expired();

  template <typename... Args>
  void object_set(Args&&... properties);

  template <typename... Args>
  void child_proxy_set(Args&&... properties);

  bool link(Element& element);

  bool link(std::list<Element>::iterator begin,
            std::list<Element>::iterator end);

  enum class PadTypes { Always, Sometime };

 protected:
  friend Pipeline;  // pipeline can access any private field
  std::unique_ptr<GstElement, Deleter<GstElement>> element{
      nullptr};  // non-owned access
  std::string name{};
  std::string alias{};
  PadTypes padType;
  bool owned{false};  // is it owned by a pipeline?
};

template <typename... Args>
void Element::object_set(Args&&... properties) {
  g_object_set(G_OBJECT(element.get()), std::forward<Args>(properties)...,
               NULL);
}

template <typename... Args>
void Element::child_proxy_set(Args&&... properties) {
  gst_child_proxy_set(GST_CHILD_PROXY(element.get()),
                      std::forward<Args>(properties)..., NULL);
}

}  // namespace vptyp