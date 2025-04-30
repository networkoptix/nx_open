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
    int androidKeyboardHeight = 0;
};

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

int Measurements::androidKeyboardHeight() const
{
    return d->androidKeyboardHeight;
}

int Measurements::getMaxTextureSize() const
{
    return d->textureSizeHelper->maxTextureSize();
}

} // namespace nx::vms::client::mobile::detail
