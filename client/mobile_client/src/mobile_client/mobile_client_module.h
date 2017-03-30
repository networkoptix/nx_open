#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <nx/utils/singleton.h>

class QnCommonModule;
struct QnMobileClientStartupParameters;

class QnMobileClientModule: public QObject, public Singleton<QnMobileClientModule>
{
public:
    QnMobileClientModule(
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    ~QnMobileClientModule();

    QnCommonModule* commonModule() const;
    ec2::AbstractECConnectionFactory* connectionFactory() const;

private:
    QnCommonModule* m_commonModule;
    std::unique_ptr<ec2::AbstractECConnectionFactory> m_connectionFactory;
};

#define qnMobileClientModule QnMobileClientModule::instance()
