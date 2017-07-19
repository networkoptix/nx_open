#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <nx/utils/singleton.h>

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

private:
    QnCommonModule* m_commonModule;
    std::unique_ptr<ec2::AbstractECConnectionFactory> m_connectionFactory;
};

#define qnClientCoreModule QnClientCoreModule::instance()
