// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::network::maintenance::log { class UploaderManager; }

namespace nx::vms::client::mobile {

class RemoteLogManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static void registerQmlType();

    RemoteLogManager(QObject* parent = nullptr);
    virtual ~RemoteLogManager() override;

    Q_INVOKABLE QString startRemoteLogSession(int durationMinutes);
    Q_INVOKABLE QString remoteLogSessionId() const;

private:
    const std::unique_ptr<network::maintenance::log::UploaderManager> m_uploader;
};

} // namespace nx::vms::client::mobile
