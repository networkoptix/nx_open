// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

class QTimer;

class QnCommonModule;

namespace nx::vms::client::core {
namespace watchers {

class NX_VMS_CLIENT_CORE_API KnownServerConnections: public QObject
{
    Q_OBJECT

public:
    struct Connection
    {
        QnUuid serverId;
        nx::utils::Url url;

        bool operator==(const Connection& other) const = default;
    };

    explicit KnownServerConnections(QnCommonModule* commonModule, QObject* parent = nullptr);
    ~KnownServerConnections();

    void start();

private:
    class Private;
    QScopedPointer<Private> const d;
};
NX_REFLECTION_INSTRUMENT(KnownServerConnections::Connection, (serverId)(url));

} // namespace watchers
} // namespace nx::vms::client::core

Q_DECLARE_METATYPE(nx::vms::client::core::watchers::KnownServerConnections::Connection)
