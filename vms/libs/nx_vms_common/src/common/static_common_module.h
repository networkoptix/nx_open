// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/singleton.h>

#include <nx/vms/api/types/connection_types.h>

#include <nx/utils/uuid.h>
#include <nx/utils/impl_ptr.h>

/**
 * Storage for the most basic and generic singletons, needed in all common projects. Initializes
 * networking and ffmpeg library, stores pool for all long-runnable threads, keeps the single time
 * source (synchronized).
 */
class NX_VMS_COMMON_API QnStaticCommonModule:
    public QObject,
    public Singleton<QnStaticCommonModule>
{
    Q_OBJECT

    using PeerType = nx::vms::api::PeerType;

public:
    QnStaticCommonModule(
        PeerType localPeerType = PeerType::notDefined,
        const QString& customCloudHost = QString());
    virtual ~QnStaticCommonModule();

    void initNetworking(const QString& customCloudHost = QString());
    void deinitNetworking();

    PeerType localPeerType() const;

    void setModuleShortId(const QnUuid& id, int number);
    int moduleShortId(const QnUuid& id) const;
    QString moduleDisplayName(const QnUuid& id) const;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

#define qnStaticCommon (QnStaticCommonModule::instance())
