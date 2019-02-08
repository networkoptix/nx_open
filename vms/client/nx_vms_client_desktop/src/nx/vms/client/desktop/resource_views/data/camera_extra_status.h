#pragma once

#include <QtCore/QMetaType>

namespace nx::vms::client::desktop {

enum class CameraExtraStatusFlag
{
    empty = 0,
    recording = 1 << 0,
    scheduled = 1 << 1,
    buggy = 1 << 2
};
Q_DECLARE_FLAGS(CameraExtraStatus, CameraExtraStatusFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CameraExtraStatus)

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::CameraExtraStatus)
