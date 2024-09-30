// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "loading_indicator.h"

#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/skin/color_theme.h>

namespace {

const static auto kAnimationLength = std::chrono::milliseconds(1000);
const static auto kFrameCount = 25;
const static auto kUpdateInterval = kAnimationLength / kFrameCount;

} // namespace

namespace nx::vms::client::desktop {

struct LoadingIndicator::Private
{
    Private(LoadingIndicator *q): q(q)
    {
        if (NX_ASSERT(QThread::currentThread() == qApp->thread())
            && pixmaps.isEmpty())
        {
            for (int i = 0; i < kFrameCount; ++i)
                pixmaps.push_back(createPixmap(1. * i / kFrameCount));
        }

        timer = new QTimer(q);
        connect(timer, &QTimer::timeout, q,
            [this, q]
            {
                frame = (frame + 1) % kFrameCount;
                emit q->frameChanged(pixmaps[frame]);
            });

        timer->start(kUpdateInterval);
    }

    LoadingIndicator *q = nullptr;
    QTimer *timer = nullptr;
    int frame = 0;

    static QList<QPixmap> pixmaps;
};
QList<QPixmap> LoadingIndicator::Private::pixmaps;

LoadingIndicator::LoadingIndicator(QObject* parent): base_type(parent), d(new Private(this))
{
}

LoadingIndicator::~LoadingIndicator()
{
}

bool LoadingIndicator::started() const
{
    return d->timer->isActive();
}

void LoadingIndicator::start()
{
    d->timer->start();
}

void LoadingIndicator::stop()
{
    d->timer->stop();
}

QPixmap LoadingIndicator::currentPixmap() const
{
    return d->pixmaps[d->frame];
}

QPixmap LoadingIndicator::createPixmap(double progress)
{
    const static double kDpiFactor = 2.0;
    const static double kImageSize = 20 * kDpiFactor;
    const static double kExternalRadius = 7 * kDpiFactor;
    const static double kInternalRadius = 5.5 * kDpiFactor;
    const static QPointF kCenter = {kImageSize / 2, kImageSize / 2};

    // Create an empty transparent image.
    QImage img(kImageSize, kImageSize, QImage::Format_RGBA64);
    img.fill(QColor::fromRgba64(0, 0, 0, 0));

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);

    // Prepare a conical gradient.
    QConicalGradient g(kCenter, 270 - 360 * progress);
    auto c1 = core::colorTheme()->color("light1");
    auto c2 = core::colorTheme()->color("light5");
    auto c3 = core::colorTheme()->color("light10");
    c2.setAlphaF(0.54);
    c3.setAlphaF(0);

    g.setColorAt(0, c1);
    g.setColorAt(0.1, c2);
    g.setColorAt(0.9, c3);

    // The internal ellipse is substracted from the external one by Qt::OddEvenFill fill rule.
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    path.addEllipse(kCenter, kExternalRadius, kExternalRadius);
    path.addEllipse(kCenter, kInternalRadius, kInternalRadius);
    painter.fillPath(path, g);

    return QPixmap::fromImage(img);
};

} // namespace nx::vms::client::desktop
