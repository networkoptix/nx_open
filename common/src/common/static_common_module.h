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
    QnStaticCommonModule(QObject *parent = nullptr);
    virtual ~QnStaticCommonModule();

    using Singleton<QnStaticCommonModule>::instance;
    using QnInstanceStorage::instance;
    using QnInstanceStorage::store;

    QnResourceDataPool *dataPool() const
    {
        return m_dataPool;
    }
protected:
    static void loadResourceData(QnResourceDataPool *dataPool, const QString &fileName, bool required);
private:
    QnResourceDataPool *m_dataPool;
};

#define qnStaticCommon (QnStaticCommonModule::instance())
