// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QTimer>
#include <QtGui/QIcon>
#include <QtWidgets/QWidget>

class QLabel;

namespace nx::vms::client::desktop {

class NotificationBellWidget : public QWidget
{
    Q_OBJECT

    enum class State
    {
        normal,
        left,
        right
    };

public:
    NotificationBellWidget(QWidget* parent = nullptr);
    virtual ~NotificationBellWidget() override;

public:
    void setAlarmState(bool enabled);
    void setIconMode(QIcon::Mode state);

private:
    void updateIconLabel();
    QSize calculateSize() const;

private:
    QTimer m_swingTimer;
    QLabel* m_iconLabel;

    QIcon m_normalIcon;
    QIcon m_rightIcon;

    State m_widgetState = State::normal;

    QIcon::Mode m_state;
};

} //namespace nx::vms::client::desktop
