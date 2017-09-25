#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <nx/utils/singleton.h>

class QnClientCoreModule;
class QnCloudStatusWatcher;
struct QnMobileClientStartupParameters;

class QnMobileClientModule: public QObject, public Singleton<QnMobileClientModule>
{
public:
    QnMobileClientModule(
        const QnMobileClientStartupParameters& startupParameters,
        QObject* parent = nullptr);
    QnCloudStatusWatcher* cloudStatusWatcher() const;
    ~QnMobileClientModule();
private:
    std::unique_ptr<QnClientCoreModule> m_clientCoreModule;
    QnCloudStatusWatcher* m_cloudStatusWatcher;
};

#define qnMobileClientModule QnMobileClientModule::instance()
