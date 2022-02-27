// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

namespace nx::vms::client::desktop {

class WidgetLimitsBackup
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

} // namespace nx::vms::client::desktop
