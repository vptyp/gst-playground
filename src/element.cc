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
    g_object_unref(sink_pad);
    return true;
  }
  bool state{false};
  // Check if the new pad's caps are compatible with the target
  GstPadLinkReturn ret = gst_pad_link(new_pad, sink_pad);
  if (GST_PAD_LINK_FAILED(ret)) {
    LOG(ERROR) << "Failed to link dynamic pad to "
               << gst_element_get_name(target);
  } else {
    LOG(INFO) << "Successfully linked dynamic pad to "
              << gst_element_get_name(target);
    state = true;
  }

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

GstPadProbeReturn Element::on_sink_pad_probe(GstPad* pad,
                                             GstPadProbeInfo* info) {
  if (GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent* event = GST_PAD_PROBE_INFO_EVENT(info);

    if (GST_EVENT_TYPE(event) == GST_EVENT_CAPS) {
      GstCaps* caps = nullptr;
      gst_event_parse_caps(event, &caps);
      if (caps && caps_callback_) {
        caps_callback_(caps);
      }
    }
  }
  return GST_PAD_PROBE_OK;
}

void Element::set_caps_callback(Element::CapsCallback callback) {
  caps_callback_ = std::move(callback);
  if (probe_id_ > 0) return;

  auto sink_pad = gst_element_get_static_pad(element.get(), "sink");
  if (!sink_pad) {
    LOG(WARNING) << "Element " << name << " has no static sink pad";
    return;
  }

  auto probeLambda = [this](GstPad* pad, GstPadProbeInfo* info) {
    return this->on_sink_pad_probe(pad, info);
  };
  using LambdaType = decltype(probeLambda);
  auto* lambdaPtr = new LambdaType(std::move(probeLambda));

  probe_id_ = gst_pad_add_probe(
      sink_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      (GstPadProbeCallback)(+[](GstPad* pad, GstPadProbeInfo* info,
                                gpointer user_data) -> GstPadProbeReturn {
        auto* func = static_cast<LambdaType*>(user_data);
        return (*func)(pad, info);
      }),
      lambdaPtr, (GDestroyNotify)(+[](gpointer data) {
        delete static_cast<LambdaType*>(data);
      }));

  gst_object_unref(sink_pad);
}

void Element::handle_dynamic_pad(Element& element) {
  GstElement* target = element.element.get();
  auto elementLambda = [this, target](GstElement* src, GstPad* pad,
                                      gpointer data) {
    this->on_pad_added(src, pad, target);
  };
  using LambdaType = decltype(elementLambda);
  auto* lambdaPtr = new LambdaType(std::move(elementLambda));
  g_signal_connect_data(
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

bool Element::link(Element& element) {
  if (padType == PadTypes::Sometime) {
    handle_dynamic_pad(element);
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

bool Element::link(std::list<Element>::iterator begin,
                   std::list<Element>::iterator end) {
  if (begin == end) return true;
  auto next = std::next(begin);

  if (padType == PadTypes::Sometime) {
    handle_dynamic_pad(*begin);
    return begin->link(next, end);
  }
  if (!gst_element_link(this->element.get(), begin->element.get())) {
    LOG(INFO) << std::format("Failed linkage of {} and {}", this->alias,
                             begin->alias);
    return false;
  }
  return begin->link(next, end);
}

void Element::reattach_probe() {
  if (probe_id_ == 0 || !element) return;

  GstPad* sink_pad = gst_element_get_static_pad(element.get(), "sink");
  if (sink_pad) {
    // Remove the old probe (which triggers its destroy notify)
    gst_pad_remove_probe(sink_pad, probe_id_);

    // Create new lambda capturing current 'this'
    auto probeLambda = [this](GstPad* pad, GstPadProbeInfo* info) {
      return this->on_sink_pad_probe(pad, info);
    };
    using LambdaType = decltype(probeLambda);
    auto* lambdaPtr = new LambdaType(std::move(probeLambda));

    // Attach new probe
    probe_id_ = gst_pad_add_probe(
        sink_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
        (GstPadProbeCallback)(+[](GstPad* pad, GstPadProbeInfo* info,
                                  gpointer user_data) -> GstPadProbeReturn {
          auto* func = static_cast<LambdaType*>(user_data);
          return (*func)(pad, info);
        }),
        lambdaPtr, (GDestroyNotify)(+[](gpointer data) {
          delete static_cast<LambdaType*>(data);
        }));

    gst_object_unref(sink_pad);
  } else {
    probe_id_ = 0;
  }
}

Element::Element(Element&& other) {
  element = std::move(other.element);
  std::swap(name, other.name);
  std::swap(alias, other.alias);
  std::swap(padType, other.padType);
  std::swap(owned, other.owned);
  std::swap(caps_callback_, other.caps_callback_);
  std::swap(probe_id_, other.probe_id_);

  if (probe_id_ > 0) {
    reattach_probe();
  }
}

Element& Element::operator=(Element&& other) {
  if (this == &other) return *this;

  element = std::move(other.element);
  name = std::move(other.name);
  alias = std::move(other.alias);
  padType = other.padType;
  owned = other.owned;
  caps_callback_ = std::move(other.caps_callback_);
  probe_id_ = other.probe_id_;

  other.padType = PadTypes::Undefined;
  other.owned = false;
  other.probe_id_ = 0;

  if (probe_id_ > 0) {
    reattach_probe();
  }

  return *this;
}
}  // namespace vptyp