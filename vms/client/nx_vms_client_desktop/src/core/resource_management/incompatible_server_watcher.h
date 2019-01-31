#pragma once

#include <QtCore/QObject>

#include <client_core/connection_context_aware.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data_fwd.h>

class QnIncompatibleServerWatcherPrivate;

class QnIncompatibleServerWatcher:
    public QObject,
    public QnConnectionContextAware
{
    Q_OBJECT

public:
    explicit QnIncompatibleServerWatcher(QObject* parent = nullptr);
    virtual ~QnIncompatibleServerWatcher() override;

    void start();
    void stop();

    /*! Prevent incompatible resource from removal if it goes offline.
     * \param id Real server GUID.
     * \param keep true to keep the resource (if present), false to not keep.
     * In case when the resource was removed when was having 'keep' flag on
     * it will be removed in this method.
     */
    void keepServer(const QnUuid& id, bool keep);

    void createInitialServers(const nx::vms::api::DiscoveredServerDataList& discoveredServers);

private:
    Q_DECLARE_PRIVATE(QnIncompatibleServerWatcher)
    QScopedPointer<QnIncompatibleServerWatcherPrivate> d_ptr;
};
