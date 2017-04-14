#include "busy_indicator_button.h"

#include <QtWidgets/QStylePainter>
#include <QtWidgets/QStyleOptionButton>

#include <ui/widgets/common/busy_indicator.h>


QnBusyIndicatorButton::QnBusyIndicatorButton(QWidget* parent) :
    base_type(parent),
    m_indicator(new QnBusyIndicatorWidget(this))
{
    m_indicator->setHidden(true);
}

QnBusyIndicatorWidget* QnBusyIndicatorButton::indicator() const
{
    return m_indicator;
}

void QnBusyIndicatorButton::resizeEvent(QResizeEvent* event)
{
    base_type::resizeEvent(event);
    m_indicator->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, m_indicator->size(), rect()));
}

void QnBusyIndicatorButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QStyleOptionButton option;
    initStyleOption(&option);

    if (isIndicatorVisible())
    {
        option.text = QString();
        option.icon = QIcon();
    }

    QStylePainter(this).drawControl(QStyle::CE_PushButton, option);
}

void QnBusyIndicatorButton::showIndicator(bool show)
{
    m_indicator->setVisible(show);
}

void QnBusyIndicatorButton::hideIndicator()
{
    showIndicator(false);
}

bool QnBusyIndicatorButton::isIndicatorVisible() const
{
    return !m_indicator->isHidden();
}

void QnBusyIndicatorButton::changeEvent(QEvent* event)
{
    base_type::changeEvent(event);

    m_indicator->setIndicatorRole(isEnabled() && isDefault()
        ? QPalette::ButtonText
        : QPalette::HighlightedText);
}
