#include "element.hh"

#include <glib.h>
#include <glog/logging.h>

#include <format>
#include <string_view>

#include "gst-bus.hh"
namespace vptyp {

Element::PadTypes checkPadType(GstElement* element) {
  GstElementClass* klass = GST_ELEMENT_GET_CLASS(element);
  GList* templates = gst_element_class_get_pad_template_list(klass);
  Element::PadTypes is_dynamic = Element::PadTypes::Always;

  for (GList* l = templates; l != nullptr; l = l->next) {
    GstPadTemplate* templ = static_cast<GstPadTemplate*>(l->data);
    if (GST_PAD_TEMPLATE_DIRECTION(templ) == GST_PAD_SRC &&
        GST_PAD_TEMPLATE_PRESENCE(templ) == GST_PAD_SOMETIMES) {
      is_dynamic = Element::PadTypes::Sometime;
      break;
    }
  }
  return is_dynamic;
}

Element::Element(std::string_view element_name, std::string_view alias)
    : name(element_name), alias(alias) {
  this->element = decltype(element)(
      gst_element_factory_make(name.data(), alias.data()), {});

  if (!this->element) {
    LOG(ERROR) << std::format("element {} was not created", element_name);
    return;
  }

  this->padType = checkPadType(this->element.get());
  LOG(INFO) << std::format("element: {}; alias: {}; created: {}; padType: {}",
                           name, alias, uint64_t(element.get()),
                           static_cast<int>(this->padType));
}

Element::~Element() {
  if (owned) {
    void* ptr = element.release();
    (void)ptr;
  }
}

bool Element::is_expired() { return !element && owned; }

bool Element::is_initialised() { return element.get(); }

bool Element::link(Element& element) {
  auto res = gst_element_link(this->element.get(), element.element.get());
  if (!res) {
    LOG(ERROR) << std::format("linkage of elements {} and {} unsuccessfull",
                              name, element.name);
    return false;
  }
  return true;
}

Element::Element(Element&& other) {
  element = std::move(other.element);
  name = other.name;
  alias = other.alias;
  padType = other.padType;
  owned = other.owned;
}

bool Element::link(std::list<Element>::iterator begin,
                   std::list<Element>::iterator end) {
  if (begin == end) return true;
  auto next = std::next(begin);

  if (padType == PadTypes::Sometime) {
    g_signal_connect(this->element.get(), "pad-added", G_CALLBACK(on_pad_added),
                     begin->element.get());
    return begin->link(next, end);
  }
  if (!gst_element_link(this->element.get(), begin->element.get())) {
    LOG(INFO) << std::format("Failed linkage of {} and {}", this->alias,
                             begin->alias);
    return false;
  }
  return begin->link(next, end);
}

}  // namespace vptyp