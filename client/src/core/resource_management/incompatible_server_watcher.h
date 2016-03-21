#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx_ec/data/api_discovery_data.h>
#include <utils/common/uuid.h>
#include <network/module_information.h>

class QnIncompatibleServerWatcherPrivate;

class QnIncompatibleServerWatcher : public QObject
{
    Q_OBJECT
public:
    explicit QnIncompatibleServerWatcher(QObject *parent = 0);
    ~QnIncompatibleServerWatcher();

    void start();
    void stop();

    /*! Prevent incompatible resource from removal if it goes offline.
     * \param id Real server GUID.
     * \param keep true to keep the resource (if present), false to not keep.
     * In case when the resource was removed when was having 'keep' flag on
     * it will be removed in this method.
     */
    void keepServer(const QnUuid &id, bool keep);

    void createInitialServers(const ec2::ApiDiscoveredServerDataList &discoveredServers);

private:
    Q_DECLARE_PRIVATE(QnIncompatibleServerWatcher)
    QScopedPointer<QnIncompatibleServerWatcherPrivate> d_ptr;
};
