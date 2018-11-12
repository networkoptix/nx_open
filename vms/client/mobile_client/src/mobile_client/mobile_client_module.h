#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <nx/utils/singleton.h>

class QnContext;
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
    virtual ~QnMobileClientModule() override;

    QnContext* context() const;

    void initDesktopCamera();

    void startLocalSearches();

private:
    std::unique_ptr<QnClientCoreModule> m_clientCoreModule;
    QnCloudStatusWatcher* m_cloudStatusWatcher = nullptr;
    QScopedPointer<QnContext> m_context;
};

#define qnMobileClientModule QnMobileClientModule::instance()
