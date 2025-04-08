// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "measurements.h"

#include <QtCore/QTimer>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>

#include <nx/build_info.h>
#include <nx/utils/guarded_callback.h>
#include <ui/texture_size_helper.h>
#include <ui/window_utils.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::mobile::detail {

struct Measurements::Private
{
    Measurements* const q;
    std::unique_ptr<QnTextureSizeHelper> textureSizeHelper;
    QMargins customMargins;
    int androidKeyboardHeight = 0;
    int deviceStatusBarHeight = 0;
};

//-------------------------------------------------------------------------------------------------

Measurements::Measurements(QQuickWindow* window, QObject* parent):
    base_type(parent),
    d{ new Private{
        .q = this,
        .textureSizeHelper = std::make_unique<QnTextureSizeHelper>(window)
    }}
{
    // Margins stuff.
    auto updateMarginsCallback =
        [this]()
        {
            // We have to update margins after all screen-change animations are finished.
            static constexpr int kUpdateDelayMs = 300;
            auto callback = [this]() { updateCustomMargins(); };
            executeDelayedParented(nx::utils::guarded(this, callback), kUpdateDelayMs, this);
        };

    connect(qApp->screens().first(), &QScreen::geometryChanged, this, updateMarginsCallback);

    // Device status bar height stuff.
    const auto screen = qApp->primaryScreen();
    connect(screen, &QScreen::orientationChanged, this,
        [this]()
        {
            const auto emitDeviceStatusBarHeightChanged = nx::utils::guarded(this,
                [this]() { emit deviceStatusBarHeightChanged(); });
            const auto kAnimationDelayMs = std::chrono::milliseconds(300);
            executeDelayedParented(emitDeviceStatusBarHeightChanged, kAnimationDelayMs, this);
        });

    if (nx::build_info::isIos())
    {
        /**
         * Workaround for iOS 16.x - there are no screen orientation change update when we
         * forcing rotation.
         */
        const auto timer = new QTimer(this);
        timer->setInterval(std::chrono::milliseconds(500));
        connect(timer, &QTimer::timeout, this, &Measurements::deviceStatusBarHeightChanged);
        timer->start();
    }

    // Android keyboard height stuff.
    if (nx::build_info::isAndroid())
    {
        const auto timer = new QTimer(this);
        timer->setInterval(std::chrono::milliseconds(100));
        connect(timer, &QTimer::timeout, timer,
            [this]()
            {
                const int value = androidKeyboardHeight();
                if (value == d->androidKeyboardHeight)
                    return;

                d->androidKeyboardHeight = value;
                emit androidKeyboardHeightChanged();
            });
        timer->start();
    }
}

Measurements::~Measurements()
{
}

QMargins Measurements::customMargins()
{
    return d->customMargins;
}

void Measurements::updateCustomMargins()
{
    const auto newMargins = getCustomMargins();
    if (d->customMargins == newMargins)
        return;

    d->customMargins = newMargins;
    emit customMarginsChanged();
}

int Measurements::deviceStatusBarHeight() const
{
    return ::statusBarHeight();
}

int Measurements::androidKeyboardHeight() const
{
    return d->androidKeyboardHeight;
}

int Measurements::getMaxTextureSize() const
{
    return d->textureSizeHelper->maxTextureSize();
}

} // namespace nx::vms::client::mobile::detail
