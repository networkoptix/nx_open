#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

#include <common/common_globals.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/instance_storage.h>

#include <nx/utils/singleton.h>
#include <nx/utils/software_version.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/types/connection_types.h>

struct QnStaticCommonModulePrivate;

/**
 * Storage for static common module's global state.
 *
 * All singletons and initialization/deinitialization code goes here.
 */
class QnStaticCommonModule:
    public QObject,
    public QnInstanceStorage,
    public Singleton<QnStaticCommonModule>
{
    Q_OBJECT

public:
    QnStaticCommonModule(
        nx::vms::api::PeerType localPeerType = nx::vms::api::PeerType::notDefined,
        const QString& brand = QString(),
        const QString& customization = QString(),
        const QString& customCloudHost = QString(),
        QObject *parent = nullptr);
    virtual ~QnStaticCommonModule();

    using Singleton<QnStaticCommonModule>::instance;
    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    nx::vms::api::PeerType localPeerType() const;
    QString brand() const;
    QString customization() const;


    void setModuleShortId(const QnUuid& id, int number);
    int moduleShortId(const QnUuid& id) const;
    QString moduleDisplayName(const QnUuid& id) const;

private:
    mutable QnMutex m_mutex;
    QMap<QnUuid, int> m_longToShortInstanceId;
    QnStaticCommonModulePrivate* m_private;

    const nx::vms::api::PeerType m_localPeerType;
    const QString m_brand;
    const QString m_customization;
};

#define qnStaticCommon (QnStaticCommonModule::instance())
