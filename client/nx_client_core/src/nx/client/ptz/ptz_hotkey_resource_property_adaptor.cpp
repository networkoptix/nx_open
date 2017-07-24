#include "ptz_hotkey_resource_property_adaptor.h"

namespace {

static const auto kNamePtzHotkeys = lit("ptzHotkeys");

} // namespace

namespace nx {
namespace client {
namespace core {
namespace ptz {

PtzHotkeysResourcePropertyAdaptor::PtzHotkeysResourcePropertyAdaptor(QObject* parent):
    base_type(kNamePtzHotkeys, QnHotkeyPtzHash(), parent)
{
}

} // namespace ptz
} // namespace core
} // namespace client
} // namespace nx

