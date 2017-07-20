#pragma once

#include <QtCore/QCoreApplication>

class QWidget;

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace messages {

class Ptz
{
    Q_DECLARE_TR_FUNCTIONS(Ptz)
public:
    static bool deletePresetInUse(QWidget* parent);

    static void failedToGetPosition(QWidget* parent, const QString& cameraName);

    static void failedToSetPosition(QWidget* parent, const QString& cameraName);
};

} // namespace messages
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
