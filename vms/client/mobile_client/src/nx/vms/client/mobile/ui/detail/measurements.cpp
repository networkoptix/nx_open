// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "measurements.h"

#include <QtCore/QTimer>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>

#include <nx/build_info.h>
#include <ui/texture_size_helper.h>
#include <ui/window_utils.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::mobile::detail {

struct Measurements::Private
{
    Measurements* const q;
    std::unique_ptr<QnTextureSizeHelper> textureSizeHelper;
    int androidKeyboardHeight = 0;
    int deviceStatusBarHeight = ::statusBarHeight();

    void updateDeviceStatusBarHeight();
};

void Measurements::Private::updateDeviceStatusBarHeight()
{
    const auto value = ::statusBarHeight();
    if (value == deviceStatusBarHeight)
        return;

    deviceStatusBarHeight = value;
    emit q->deviceStatusBarHeightChanged();
}

//-------------------------------------------------------------------------------------------------

Measurements::Measurements(QQuickWindow* window, QObject* parent):
    base_type(parent),
    d{ new Private{
        .q = this,
        .textureSizeHelper = std::make_unique<QnTextureSizeHelper>(window)
    }}
{
    // Android keyboard height stuff.
    if (nx::build_info::isAndroid())
    {
        const auto timer = new QTimer(this);
        timer->setInterval(std::chrono::milliseconds(100));
        connect(timer, &QTimer::timeout, timer,
            [this]()
            {
                const int value = ::androidKeyboardHeight();
                if (value == d->androidKeyboardHeight)
                    return;

                d->androidKeyboardHeight = value;
                emit androidKeyboardHeightChanged();
            });
        timer->start();
    }

    const auto updateStatusBarHeight = [this]() { d->updateDeviceStatusBarHeight(); };

    // Device status bar height stuff.
    const auto screen = qApp->primaryScreen();
    connect(screen, &QScreen::orientationChanged, this,
        [this, updateStatusBarHeight]()
        {
            const auto kAnimationDelayMs = std::chrono::milliseconds(300);
            executeDelayedParented(updateStatusBarHeight, kAnimationDelayMs, this);
        });


    if (nx::build_info::isIos())
    {
        /**
         * Workaround for iOS 16.x - there are no screen orientation change update when we
         * force rotation.
         */
        const auto timer = new QTimer(this);
        timer->setInterval(std::chrono::milliseconds(500));
        connect(timer, &QTimer::timeout, this, updateStatusBarHeight);
        timer->start();
    }
}

Measurements::~Measurements()
{
}

int Measurements::androidKeyboardHeight() const
{
    return d->androidKeyboardHeight;
}

int Measurements::deviceStatusBarHeight() const
{
    return ::statusBarHeight();
}

int Measurements::getMaxTextureSize() const
{
    return d->textureSizeHelper->maxTextureSize();
}

} // namespace nx::vms::client::mobile::detail
