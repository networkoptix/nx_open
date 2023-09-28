// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "hotkey_resource_property_adaptor.h"

namespace {

static const auto kNamePtzHotkeys = "ptzHotkeys";

} // namespace

namespace nx::vms::client::core {
namespace ptz {

HotkeysResourcePropertyAdaptor::HotkeysResourcePropertyAdaptor(QObject* parent):
    base_type(kNamePtzHotkeys, PresetIdByHotkey(), parent)
{
}

} // namespace ptz
} // namespace nx::vms::client::core
