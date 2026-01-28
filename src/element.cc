#include "element.hh"

#include <glib.h>
#include <glog/logging.h>

#include <format>
#include <string_view>

#include "glib-object.h"

namespace vptyp {

bool Element::on_pad_added(GstElement* src, GstPad* new_pad,
                           GstElement* target) {
  GstPad* sink_pad = gst_element_get_static_pad(target, "sink");

  // Check if the sink is already linked (e.g., if multiple pads are found)
  if (gst_pad_is_linked(sink_pad)) {
    LOG(INFO) << "Sink pad is already linked. Ignoring.";
    g_object_unref(sink_pad);
    return true;
  }
  bool state{false};
  auto name = gst_element_get_name(target);
  // Check if the new pad's caps are compatible with the target
  GstPadLinkReturn ret = gst_pad_link(new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED(ret)) {
    LOG(ERROR) << "Failed to link dynamic pad to " << name;
  } else {
    LOG(INFO) << "Successfully linked dynamic pad to " << name;
    state = true;
  }
  g_free(name);

  g_object_unref(sink_pad);
  return state;
}

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

bool Element::is_initialised() { return element.get(); }

GstPadProbeReturn Element::on_sink_pad_probe(GstPad* pad,
                                             GstPadProbeInfo* info) {
  if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);

    if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
      GstCaps* caps = nullptr;
      gst_event_parse_caps(event, &caps);
      if (caps && capsCallback) {
        capsCallback(caps);
      }
    }
  }
  return GST_PAD_PROBE_OK;
}

void Element::add_caps_probe(GstPad* pad) {
  auto probeLambda = [this](GstPad* pad, GstPadProbeInfo* info) {
    return this->on_sink_pad_probe(pad, info);
  };
  using LambdaType = decltype(probeLambda);
  auto* lambdaPtr = new LambdaType(std::move(probeLambda));

  probeId = gst_pad_add_probe(
      pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback)(+[](GstPad* pad, GstPadProbeInfo* info,
                                gpointer user_data) -> GstPadProbeReturn {
        auto* func = static_cast<LambdaType*>(user_data);
        return (*func)(pad, info);
      }),
      lambdaPtr, (GDestroyNotify)(+[](gpointer data) {
        delete static_cast<LambdaType*>(data);
      }));
}

void Element::set_caps_callback(Element::CapsCallback callback) {
  capsCallback = std::move(callback);
  if (probeId > 0) return;

  GstPad* sink_pad = gst_element_get_static_pad(element.get(), "sink");
  if (!sink_pad) {
    LOG(WARNING) << "Element " << name << " has no static sink pad";
    return;
  }

  add_caps_probe(sink_pad);
  gst_object_unref(sink_pad);
}

void Element::remove_probe() {
  if (probeId == 0 || !element) return;

  GstPad* sink_pad = gst_element_get_static_pad(element.get(), "sink");
  if (sink_pad) {
    gst_pad_remove_probe(sink_pad, probeId);
    gst_object_unref(sink_pad);
    probeId = 0;
  }
}

void Element::reattach_probe() {
  if (probeId == 0 || !element) return;
  remove_probe();
  GstPad* sink_pad = gst_element_get_static_pad(element.get(), "sink");
  if (sink_pad) {
    add_caps_probe(sink_pad);
  } else {
    probeId = 0;
  }
}

void Element::handle_dynamic_pad(GstElement* element) {
  GstElement* target = element;
  pendingDynamicTarget = target;

  auto elementLambda = [this, target](GstElement* src, GstPad* pad,
                                      gpointer data) {
    this->on_pad_added(src, pad, target);
  };
  using LambdaType = decltype(elementLambda);
  auto* lambdaPtr = new LambdaType(std::move(elementLambda));

  padAddedSignalId = g_signal_connect_data(
      this->element.get(), "pad-added",
      reinterpret_cast<void (*)()>(
          +[](GstElement* src, GstPad* pad, gpointer data) {
            auto* func = static_cast<LambdaType*>(data);
            (*func)(src, pad, nullptr);
          }),
      lambdaPtr,
      +[](gpointer data, GClosure* closure) {
        delete static_cast<LambdaType*>(data);
      },
      GConnectFlags(0));

  LOG(INFO) << "linkage would be dynamically handled";
}

void Element::reattach_dynamic_pad_handler() {
  if (padAddedSignalId == 0 || !element || !pendingDynamicTarget) return;

  // Disconnect old signal
  g_signal_handler_disconnect(element.get(), padAddedSignalId);

  handle_dynamic_pad(pendingDynamicTarget);
}

bool Element::link(Element& element) {
  if (padType == PadTypes::Sometime) {
    handle_dynamic_pad(element.element.get());
    return true;
  }

  auto res = gst_element_link(this->element.get(), element.element.get());
  if (!res) {
    LOG(ERROR) << std::format("linkage of elements {} and {} unsuccessfull",
                              name, element.name);
    return false;
  }
  return true;
}

bool Element::link(std::list<std::unique_ptr<Element>>::iterator begin,
                   std::list<std::unique_ptr<Element>>::iterator end) {
  if (begin == end) return true;
  auto next = std::next(begin);

  if (padType == PadTypes::Sometime) {
    handle_dynamic_pad((*begin)->element.get());
    return (*begin)->link(next, end);
  }
  if (!gst_element_link(this->element.get(), (*begin)->element.get())) {
    LOG(INFO) << std::format("Failed linkage of {} and {}", this->alias,
                             (*begin)->alias);
    return false;
  }
  return (*begin)->link(next, end);
}

Element::Element(std::string_view element_name, std::string_view alias)
    : name(element_name), alias(alias) {
  this->element = make_gst(gst_element_factory_make(name.data(), alias.data()));
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
  if (padAddedSignalId > 0)
    g_signal_handler_disconnect(element.get(), padAddedSignalId);

  remove_probe();
}

Element::Element(Element&& other) {
  element = std::move(other.element);
  std::swap(name, other.name);
  std::swap(alias, other.alias);
  std::swap(padType, other.padType);
  std::swap(capsCallback, other.capsCallback);
  std::swap(probeId, other.probeId);
  std::swap(padAddedSignalId, other.padAddedSignalId);
  std::swap(pendingDynamicTarget, other.pendingDynamicTarget);

  if (probeId > 0) {
    reattach_probe();
  }
  if (padAddedSignalId > 0) {
    reattach_dynamic_pad_handler();
  }
}

Element& Element::operator=(Element&& other) {
  if (this == &other) return *this;

  if (padAddedSignalId > 0)
    g_signal_handler_disconnect(element.get(), padAddedSignalId);
  remove_probe();

  element = std::move(other.element);
  name = std::move(other.name);
  alias = std::move(other.alias);
  padType = other.padType;
  capsCallback = std::move(other.capsCallback);
  probeId = other.probeId;
  padAddedSignalId = other.padAddedSignalId;
  pendingDynamicTarget = other.pendingDynamicTarget;

  other.padType = PadTypes::Undefined;
  other.probeId = 0;
  other.padAddedSignalId = 0;
  other.pendingDynamicTarget = nullptr;

  if (probeId > 0) {
    reattach_probe();
  }
  if (padAddedSignalId > 0) {
    reattach_dynamic_pad_handler();
  }

  return *this;
}
}  // namespace vptyp