#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <nx/utils/singleton.h>

class QnClientCoreModule;
struct QnMobileClientStartupParameters;

class QnMobileClientModule: public QObject, public Singleton<QnMobileClientModule>
{
public:
    QnMobileClientModule(
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    ~QnMobileClientModule();
private:
    QnClientCoreModule* m_clientCoreModule;
};

#define qnMobileClientModule QnMobileClientModule::instance()
