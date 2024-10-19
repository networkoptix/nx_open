// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QObject>

namespace nx::vms::client::mobile {

class RemoteLogManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static void registerQmlType();
    static RemoteLogManager* instance();

    RemoteLogManager(QObject* parent = nullptr);
    virtual ~RemoteLogManager() override;

    Q_INVOKABLE QString startRemoteLogSession(int durationMinutes);
    Q_INVOKABLE QString remoteLogSessionId() const;
};

} // namespace nx::vms::client::mobile
