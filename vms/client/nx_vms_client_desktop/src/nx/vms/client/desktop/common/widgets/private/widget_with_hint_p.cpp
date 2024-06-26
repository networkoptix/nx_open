// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "widget_with_hint_p.h"

#include <QtCore/QEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/private/qwidget_p.h>

#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/widgets/word_wrapped_label.h>

namespace nx::vms::client::desktop {

WidgetWithHintPrivate::WidgetWithHintPrivate(QWidget* q) :
    q(q),
    m_button(new HintButton(q)),
    m_spacing(style::Metrics::kHintButtonMargin)
{
    m_button->setHidden(true);
    m_button->setFocusPolicy(Qt::NoFocus);
}

WidgetWithHintPrivate::~WidgetWithHintPrivate()
{
}

QString WidgetWithHintPrivate::hint() const
{
    return m_button->hintText();
}

void WidgetWithHintPrivate::setHint(const QString& value)
{
    m_button->setHintText(value.trimmed());
    const bool noHint = hint().isEmpty();
    if (m_button->isHidden() == noHint)
        return;

    m_button->setHidden(noHint);
    updateContentsMargins();
}

void WidgetWithHintPrivate::addHintLine(const QString& value)
{
    m_button->addHintLine(value.trimmed());
    const bool noHint = hint().isEmpty();
    if (m_button->isHidden() == noHint)
        return;

    m_button->setHidden(noHint);
    updateContentsMargins();
}

int WidgetWithHintPrivate::spacing() const
{
    return m_spacing;
}

void WidgetWithHintPrivate::setSpacing(int value)
{
    if (m_spacing == value)
        return;

    m_spacing = value;
    updateContentsMargins();
}

QWidget* WidgetWithHintPrivate::parentObject()
{
    auto wrappedLabel = qobject_cast<QnWordWrappedLabel*>(q);
    return wrappedLabel ? wrappedLabel->label() : q;
}

void WidgetWithHintPrivate::handleResize()
{
    auto rect = q->rect();
    Qt::Alignment alignment = Qt::AlignLeft;

    const auto parent = parentObject();
    if (auto label = qobject_cast<QLabel*>(parent))
        alignment = label->alignment();

    if (!alignment.testFlag(Qt::AlignRight))
        rect.setWidth(parent->sizeHint().width()); //< Already includes added width.

    const auto buttonGeometry = QStyle::alignedRect(
        parent->layoutDirection(),
        Qt::AlignRight | Qt::AlignVCenter,
        m_button->size(),
        rect);

    m_button->setGeometry(buttonGeometry);

    if (!m_button->isHidden())
        updateContentsMargins();
}

QSize WidgetWithHintPrivate::minimumSizeHint(const QSize& base) const
{
    return m_button->isHidden() ? base : base.expandedTo(m_button->size());
}

void WidgetWithHintPrivate::updateContentsMargins()
{
    if (m_button->isHidden())
    {
        q->setContentsMargins(0, 0, 0, 0);
    }
    else
    {
        const auto width = m_button->width() + m_spacing;
        auto parent = parentObject();
        const auto height = qMax(0, m_button->height() - parent->height());
        const auto top = height / 2;
        const auto bottom = height - top;

        if (parent->layoutDirection() == Qt::RightToLeft)
            parent->setContentsMargins(width, top, 0, bottom);
        else
            parent->setContentsMargins(0, top, width, bottom);
    }
}

} // namespace nx::vms::client::desktop
