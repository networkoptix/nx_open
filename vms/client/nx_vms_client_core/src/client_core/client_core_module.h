// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QObject>

#include <nx/core/access/access_types.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/serialization/format.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>

class QQmlEngine;

class QnCommonModule;
class QnDataProviderFactory;
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
    public nx::vms::client::core::SystemContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnClientCoreModule(nx::vms::client::core::SystemContext* systemContext);
    virtual ~QnClientCoreModule() override;

    static QnClientCoreModule* instance();

    void initializeNetworking(
        nx::vms::api::PeerType peerType,
        Qn::SerializationFormat serializationFormat);

    nx::vms::client::core::NetworkModule* networkModule() const;

    QnCommonModule* commonModule() const;
    QnDataProviderFactory* dataProviderFactory() const;
    nx::vms::client::core::SessionTokenTerminator* sessionTokenTerminator() const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

#define qnClientCoreModule QnClientCoreModule::instance()
