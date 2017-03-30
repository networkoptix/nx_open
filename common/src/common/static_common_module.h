#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QDateTime>

#include <core/resource/resource_fwd.h>

#include <nx/utils/singleton.h>
#include <utils/common/instance_storage.h>

class QnResourceDataPool;

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
    QnStaticCommonModule(Qn::PeerType localPeerType,
        const QString& brand,
        const QString& customization,
        QObject *parent = nullptr);
    virtual ~QnStaticCommonModule();

    using Singleton<QnStaticCommonModule>::instance;
    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    Qn::PeerType localPeerType() const;
    QString brand() const;
    QString customization() const;

    QnResourceDataPool *dataPool() const;
protected:
    static void loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required);
private:
    const Qn::PeerType m_localPeerType;
    const QString m_brand;
    const QString m_customization;

    QnResourceDataPool *m_dataPool = nullptr;
};

#define qnStaticCommon (QnStaticCommonModule::instance())
