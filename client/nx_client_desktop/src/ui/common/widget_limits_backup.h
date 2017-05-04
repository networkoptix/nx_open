#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>


class QnWidgetLimitsBackup
{
public:
    void backup(QWidget* widget)
    {
        m_widget = widget;
        if (!widget)
            return;

        m_minimumSize = widget->minimumSize();
        m_maximumSize = widget->maximumSize();
    }

    void restore()
    {
        if (!m_widget)
            return;

        m_widget->setMinimumSize(m_minimumSize);
        m_widget->setMaximumSize(m_maximumSize);
    }

private:
    QPointer<QWidget> m_widget;
    QSize m_minimumSize;
    QSize m_maximumSize;
};
