// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

class QAction;

namespace nx::vms::client::desktop {

class ScreenRecordingWatcher: public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isOn READ isOn NOTIFY stateChanged)

public:
    explicit ScreenRecordingWatcher();
    static void registerQmlType();

    bool isOn() const;

signals:
    void stateChanged();

private:
    const QPointer<QAction> m_screenRecordingAction;
};

} // namespace nx::vms::client::desktop
