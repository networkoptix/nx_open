// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QDate>

namespace nx::vms::client::desktop {

/*
 * Periodically checks QDate::currentDate() and emits a signal every time it changes.
 */
class CurrentDateMonitor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    CurrentDateMonitor(QObject* parent);
    virtual ~CurrentDateMonitor() = default;

signals:
    void currentDateChanged();

private:
    QDate m_prevDate;
};

} // namespace nx::vms::client::desktop
