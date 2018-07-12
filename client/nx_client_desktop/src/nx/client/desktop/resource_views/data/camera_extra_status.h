#pragma once

#include <QtCore/QMetaType>

namespace nx {
namespace client {
namespace desktop {

enum class CameraExtraStatusFlag
{
    empty = 0,
    recording = 1 << 0,
    scheduled = 1 << 1,
    buggy = 1 << 2
};
Q_DECLARE_FLAGS(CameraExtraStatus, CameraExtraStatusFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(CameraExtraStatus)

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::CameraExtraStatus)
