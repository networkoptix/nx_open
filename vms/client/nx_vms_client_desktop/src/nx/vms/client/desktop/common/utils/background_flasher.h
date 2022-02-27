// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>
#include <QtGui/QColor>

#include <nx/utils/impl_ptr.h>

class QWidget;

namespace nx::vms::client::desktop {

class BackgroundFlasher: public QObject
{
    Q_OBJECT

public:
    static void flash(
        QWidget* widget, //< Widget to flash.
        const QColor& color, //< Flashing color, alpha is ignored.
        qreal opacity = 0.2, //< Maximum opacity.
        std::chrono::milliseconds duration = std::chrono::seconds(1), //< Full duration.
        int iterations = 2); //< Number of flashes.

private:
    BackgroundFlasher(QWidget* parent);
    virtual ~BackgroundFlasher() override;

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
