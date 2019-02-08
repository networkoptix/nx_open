#include "ptz_hotkey_resource_property_adaptor.h"

namespace {

static const auto kNamePtzHotkeys = lit("ptzHotkeys");

} // namespace

namespace nx::vms::client::core {
namespace ptz {

PtzHotkeysResourcePropertyAdaptor::PtzHotkeysResourcePropertyAdaptor(QObject* parent):
    base_type(kNamePtzHotkeys, QnPtzIdByHotkeyHash(), parent)
{
}

} // namespace ptz
} // namespace nx::vms::client::core

