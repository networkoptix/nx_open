#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <nx/utils/singleton.h>

class QQmlEngine;

class QnCommonModule;
class QnPtzControllerPool;
class QnLayoutTourStateManager;
class QnDataProviderFactory;

class QnClientCoreModule: public QObject, public Singleton<QnClientCoreModule>
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnClientCoreModule(QObject* parent = nullptr);
    ~QnClientCoreModule();

    QnCommonModule* commonModule() const;
    ec2::AbstractECConnectionFactory* connectionFactory() const;
    QnPtzControllerPool* ptzControllerPool() const;
    QnLayoutTourStateManager* layoutTourStateManager() const;
    QnDataProviderFactory* dataProviderFactory() const;

    QQmlEngine* mainQmlEngine();

private:
    void registerResourceDataProviders();

private:
    QnCommonModule* m_commonModule;
    std::unique_ptr<ec2::RemoteConnectionFactory> m_connectionFactory;
    QQmlEngine* m_qmlEngine = nullptr;
    QScopedPointer<QnDataProviderFactory> m_resourceDataProviderFactory;
};

#define qnClientCoreModule QnClientCoreModule::instance()
