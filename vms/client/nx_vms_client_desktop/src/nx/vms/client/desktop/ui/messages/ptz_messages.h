// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

class QWidget;

namespace nx::vms::client::desktop {
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
} // namespace nx::vms::client::desktop
