#include "widget_with_hint_p.h"

#include <QtCore/QEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QStyle>
#include <QtWidgets/private/qwidget_p.h>

#include <ui/style/helper.h>

#include <nx/vms/client/desktop/common/widgets/hint_button.h>

namespace nx::vms::client::desktop {

WidgetWithHintPrivate::WidgetWithHintPrivate(QWidget* q) :
    q(q),
    m_button(new HintButton(q)),
    m_spacing(style::Metrics::kHintButtonMargin)
{
    m_button->setHidden(true);
}

WidgetWithHintPrivate::~WidgetWithHintPrivate()
{
}

QString WidgetWithHintPrivate::hint() const
{
    return m_button->hint();
}

void WidgetWithHintPrivate::setHint(const QString& value)
{
    m_button->setHint(value.trimmed());
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

void WidgetWithHintPrivate::handleResize()
{
    auto rect = q->rect();
    Qt::Alignment alignment = Qt::AlignLeft;

    if (auto label = qobject_cast<QLabel*>(q))
        alignment = label->alignment();

    if (!alignment.testFlag(Qt::AlignRight))
        rect.setWidth(q->sizeHint().width()); //< Already includes added width.

    const auto buttonGeometry = QStyle::alignedRect(
        q->layoutDirection(),
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
        const auto height = qMax(0, m_button->height() - q->height());
        const auto top = height / 2;
        const auto bottom = height - top;

        if (q->layoutDirection() == Qt::RightToLeft)
            q->setContentsMargins(width, top, 0, bottom);
        else
            q->setContentsMargins(0, top, width, bottom);
    }
}

} // namespace nx::vms::client::desktop
