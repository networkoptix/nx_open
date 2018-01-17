#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <nx/utils/singleton.h>

class QQmlEngine;

class QnCommonModule;
class QnPtzControllerPool;
class QnLayoutTourStateManager;

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

    QQmlEngine* mainQmlEngine();

private:
    QnCommonModule* m_commonModule;
    std::unique_ptr<ec2::AbstractECConnectionFactory> m_connectionFactory;
    QQmlEngine* m_qmlEngine = nullptr;
};

#define qnClientCoreModule QnClientCoreModule::instance()
