// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "background_flasher.h"

#include <QtCore/QVariantAnimation>
#include <QtCore/QSequentialAnimationGroup>
#include <QtWidgets/QWidget>

#include <ui/common/palette.h>
#include <utils/math/color_transformations.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;

class BackgroundFlasher::Private
{
    BackgroundFlasher* const q;

public:
    Private(BackgroundFlasher* q):
        q(q),
        m_group(new QSequentialAnimationGroup(q)),
        m_in(new QVariantAnimation(m_group)),
        m_out(new QVariantAnimation(m_group))
    {
        m_group->addAnimation(m_in);
        m_group->addAnimation(m_out);

        m_in->setStartValue(0.0);
        m_out->setEndValue(0.0);

        connect(m_in, &QVariantAnimation::valueChanged, q,
            [this](const QVariant& value) { updateAlpha(value.toReal()); });

        connect(m_out, &QVariantAnimation::valueChanged, q,
            [this](const QVariant& value) { updateAlpha(value.toReal()); });
    }

    void flash(const QColor& color, qreal opacity, std::chrono::milliseconds halfPeriod,
        int iterations)
    {
        m_color = color;

        m_in->setEndValue(opacity);
        m_in->setDuration(halfPeriod.count());

        m_out->setStartValue(opacity);
        m_out->setDuration(halfPeriod.count());

        m_group->setLoopCount(m_group->state() == QAbstractAnimation::Running
            ? (m_group->currentLoop() + iterations)
            : iterations);

        m_group->start();
    }

private:
    void updateAlpha(qreal value)
    {
        if (m_group->state() != QAbstractAnimation::Running)
            return;

        if (const auto widget = qobject_cast<QWidget*>(q->parent()))
            setPaletteColor(widget, widget->backgroundRole(), toTransparent(m_color, value));
    }

private:
    QSequentialAnimationGroup* const m_group;
    QVariantAnimation* const m_in;
    QVariantAnimation* const m_out;
    QColor m_color;
};

void BackgroundFlasher::flash(
    QWidget* widget,
    const QColor& color,
    qreal opacity,
    std::chrono::milliseconds duration,
    int iterations)
{
    if (!NX_ASSERT(widget))
        return;

    if (!NX_ASSERT(iterations > 0))
        iterations = 1;

    auto halfPeriod = duration / (iterations * 2);
    if (!NX_ASSERT(halfPeriod > 0ms, "Duration is too short"))
        halfPeriod = 1ms; //< Safety fallback.

    auto flasher = widget->findChild<BackgroundFlasher*>();
    if (!flasher)
        flasher = new BackgroundFlasher(widget);

    flasher->d->flash(color, opacity, halfPeriod, iterations);
}

BackgroundFlasher::BackgroundFlasher(QWidget* parent):
    QObject(parent),
    d(new Private(this))
{
}

BackgroundFlasher::~BackgroundFlasher()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace nx::vms::client::desktop
