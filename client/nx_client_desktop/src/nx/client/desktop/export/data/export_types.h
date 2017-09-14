#pragma once

#include <QtCore/QMetaType>

namespace nx {
namespace client {
namespace desktop {

struct ExportMediaSettings;
struct ExportLayoutSettings;

enum class ExportProcessStatus
{
    initial,
    exporting,
    success,
    failure,
    cancelling,
    cancelled
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::ExportProcessStatus)
