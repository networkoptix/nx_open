// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/core/access/access_types.h>
#include <nx/fusion/serialization_format.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

class QQmlEngine;

class QnCloudStatusWatcher;
class QnCommonModule;
class QnDataProviderFactory;
class QnLayoutTourStateManager;
class QnPtzControllerPool;
class QnResourcePool;

namespace nx::vms::api { enum class PeerType; }
namespace nx::vms::client::core {

class NetworkModule;
class SessionTokenTerminator;
class SystemContext;

} // namespace nx::vms::client::core

class NX_VMS_CLIENT_CORE_API QnClientCoreModule:
    public QObject,
    public Singleton<QnClientCoreModule>
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnClientCoreModule(nx::vms::client::core::SystemContext* systemContext);
    virtual ~QnClientCoreModule() override;

    void initializeNetworking(
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat);

    nx::vms::client::core::NetworkModule* networkModule() const;

    /**
     * Main Resource Pool of the client.
     */
    QnResourcePool* resourcePool() const;
    QnCommonModule* commonModule() const;
    QnLayoutTourStateManager* layoutTourStateManager() const;
    QnDataProviderFactory* dataProviderFactory() const;
    nx::vms::client::core::SessionTokenTerminator* sessionTokenTerminator() const;

    QQmlEngine* mainQmlEngine() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

#define qnClientCoreModule QnClientCoreModule::instance()
