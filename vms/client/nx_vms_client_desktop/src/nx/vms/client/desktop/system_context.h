// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context.h>

#include "system_context_aware.h" //< Forward declarations.

class QnCameraBookmarksManager;
class QnCameraDataManager;
class QnMediaServerStatisticsManager;
class QnServerStorageManager;

namespace nx::vms::client::desktop {

class LayoutSnapshotManager;
class LdapStatusWatcher;
class LogsManagementWatcher;
class RestApiHelper;
class ShowreelStateManager;
class ServerRuntimeEventConnector;
class SystemSpecificLocalSettings;
class TrafficRelayUrlWatcher;
class VideoCache;
class VideoWallOnlineScreensWatcher;
class VirtualCameraManager;
class NonEditableUsersAndGroups;

namespace analytics { class TaxonomyManager; }

class NX_VMS_CLIENT_DESKTOP_API SystemContext: public core::SystemContext
{
    Q_OBJECT
    using base_type = core::SystemContext;

public:
    /**
     * Initialize client-level System Context based on existing client-core-level System Context.
     * Destruction order must be handled by the caller.
     * @param peerId Id of the current peer in the Message Bus. It is persistent and is not changed
     *     between the application runs. Desktop Client calculates actual peer id depending on the
     *     stored persistent id and on the number of the running client instance, so different
     *     Client windows have different peer ids.
     */
    SystemContext(Mode mode, nx::Uuid peerId, QObject* parent = nullptr);
    virtual ~SystemContext() override;

    static void registerQmlType();
    static SystemContext* fromResource(const QnResourcePtr& resource);

    VideoWallOnlineScreensWatcher* videoWallOnlineScreensWatcher() const;
    LdapStatusWatcher* ldapStatusWatcher() const;
    NonEditableUsersAndGroups* nonEditableUsersAndGroups() const;
    ServerRuntimeEventConnector* serverRuntimeEventConnector() const;
    QnServerStorageManager* serverStorageManager() const;
    QnCameraBookmarksManager* cameraBookmarksManager() const;
    QnCameraDataManager* cameraDataManager() const;
    VirtualCameraManager* virtualCameraManager() const;
    VideoCache* videoCache() const;
    LayoutSnapshotManager* layoutSnapshotManager() const;
    ShowreelStateManager* showreelStateManager() const;
    LogsManagementWatcher* logsManagementWatcher() const;
    QnMediaServerStatisticsManager* mediaServerStatisticsManager() const;
    SystemSpecificLocalSettings* localSettings() const;
    RestApiHelper* restApiHelper() const;
    Q_INVOKABLE nx::vms::client::desktop::analytics::TaxonomyManager* taxonomyManager() const;
    virtual nx::vms::common::SessionTokenHelperPtr getSessionTokenHelper() const override;
    TrafficRelayUrlWatcher* trafficRelayUrlWatcher() const;

protected:
    virtual void setMessageProcessor(QnCommonMessageProcessor* messageProcessor) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::SystemContext)
