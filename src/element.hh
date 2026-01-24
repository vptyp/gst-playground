#pragma once
#include <gst/gst.h>

#include <functional>
#include <list>
#include <memory>
#include <string_view>
#include <type_traits>

#include "gst/gstelement.h"
#include "gstDeleter.hh"
namespace vptyp {

class Pipeline;

class Element {
 public:
  Element(std::string_view element_name, std::string_view alias);  // factory
  Element(Element&& other);
  Element& operator=(Element&& other);
  Element(Element& other) = delete;
  Element& operator=(Element&) = delete;
  virtual ~Element();

  bool is_initialised();
  bool is_expired();

  template <typename... Args>
  void object_set(Args&&... properties);

  template <typename ValueType>
  ValueType object_get(std::string_view name) const;

  template <typename... Args>
  void child_proxy_set(Args&&... properties);

  bool link(Element& element);

  bool link(std::list<Element>::iterator begin,
            std::list<Element>::iterator end);

  using CapsCallback = std::function<void(GstCaps*)>;
  void set_caps_callback(CapsCallback callback);

  enum class PadTypes { Undefined, Always, Sometime };

 protected:
  virtual GstPadProbeReturn on_sink_pad_probe(GstPad* pad,
                                              GstPadProbeInfo* info);
  virtual bool on_pad_added(GstElement* src, GstPad* new_pad,
                            GstElement* target);

  virtual void handle_dynamic_pad(Element& element);
  void reattach_probe();

 protected:
  friend Pipeline;  // pipeline can access any private field
  std::unique_ptr<GstElement, Deleter<GstElement>> element{
      nullptr};  // non-owned access
  std::string name{};
  std::string alias{};
  PadTypes padType{PadTypes::Undefined};
  bool owned{false};  // is it owned by a pipeline?
  CapsCallback caps_callback_{nullptr};
  gulong probe_id_{0};
};

template <typename... Args>
void Element::object_set(Args&&... properties) {
  g_object_set(G_OBJECT(element.get()), std::forward<Args>(properties)...,
               NULL);
}

template <typename ValueType>
ValueType Element::object_get(std::string_view name) const {
  static_assert(std::is_default_constructible<ValueType>::value,
                "Type should be default constructible for object_get");

  ValueType value{};
  g_object_get(G_OBJECT(element.get()), name.data(), &value, nullptr);
  return value;
}

template <typename... Args>
void Element::child_proxy_set(Args&&... properties) {
  gst_child_proxy_set(GST_CHILD_PROXY(element.get()),
                      std::forward<Args>(properties)..., NULL);
}

}  // namespace vptyp