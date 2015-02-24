#ifndef CLIENT_AXHDWITNESS_H_
#define CLIENT_AXHDWITNESS_H_

#include <QtCore/QScopedPointer>

#include <ActiveQt/QAxBindable>

#include "client/client_resource_processor.h"
#include "ui/widgets/main_window.h"
#include "ui/style/skin.h"
#include <client/client_module.h>
#include "ui/style/noptix_style.h"
#include "ui/customization/customizer.h"
#include "nx_ec/ec_api.h"
#include "api/abstract_connection.h"

class QnLongRunnablePool;
class QnSyncTime;
class QnPlatformAbstraction;
class QnClientPtzControllerPool;
class QnClientPtzControllerPool;
class QnGlobalSettings;
class QnRuntimeInfoManager;
class QnClientMessageProcessor;
class QnModuleFinder;
class QnRouter;
class QnGlobalModuleFinder;
class QnServerInterfaceWatcher;
class QnResourcePropertyDictionary;
class QnResourceStatusDictionary;
class QnCameraUserAttributePool;
class QnMediaServerUserAttributesPool;
class TimerManager;
class QnServerCameraFactory;

class AxHDWitness : public QWidget
{
    Q_OBJECT

    Q_CLASSINFO("ClassID", "{5E813AE3-F318-4814-ABE0-59ECD0CAFBC8}");
    Q_CLASSINFO("InterfaceID", "{4DB301D1-DB3C-4746-8A92-CEDA4322018A}");
    Q_CLASSINFO("EventsID", "{78AA2C5E-1115-405C-A8D6-BDE8CF00309A}");

public:
    AxHDWitness(QWidget* parent, const char* name = 0);
    ~AxHDWitness();

public slots:
    void initialize();
    void finalize();

    void jumpToLive();
    void play();
    void pause();
    void nextFrame();
    void prevFrame();
    void clear();

    void setSpeed(double speed);

    qint64 currentTime();
    void setCurrentTime(qint64 time);

    void addResourceToLayout(const QString &uniqueId, qint64 timestamp);
    void addResourcesToLayout(const QString &uniqueIds, qint64 timestamp);
    void removeFromCurrentLayout(const QString &uniqueId);

    void reconnect(const QString &url);

    void maximizeItem(const QString &uniqueId);
    void unmaximizeItem(const QString &uniqueId);

    void slidePanelsOut();

    QString resourceListXml();

signals:
    void connectedProcessed();

private slots:
    void at_connectProcessed();
    void at_initialResourcesReceived(const QnUserResourcePtr &);

protected:
    virtual bool event(QEvent *event) override;

private:
    bool doInitialize();
    void doFinalize();
    void createMainWindow();

private:
    QUrl m_url;
    QnEc2ConnectionRequestResult m_result;
    int m_connectingHandle;

    bool m_isInitialized;

	QScopedPointer<ec2::AbstractECConnectionFactory> m_ec2ConnectionFactory;
    QnClientResourceProcessor m_clientResourceProcessor;

    QScopedPointer<QnServerCameraFactory> m_serverCameraFactory;

    QScopedPointer<QnWorkbenchContext> m_context;
	QScopedPointer<QnSkin> skin;
	QScopedPointer<QnCustomizer> customizer;
	QScopedPointer<QnClientModule> m_clientModule;
	QScopedPointer<QnSyncTime> m_syncTime;

	QScopedPointer<QnPlatformAbstraction> m_platform;
    QScopedPointer<QnLongRunnablePool> m_runnablePool;
    QScopedPointer<QnClientPtzControllerPool> m_clientPtzPool;
    QScopedPointer<QnGlobalSettings> m_globalSettings;
    QScopedPointer<QnClientMessageProcessor> m_clientMessageProcessor;
    QScopedPointer<QnRuntimeInfoManager> m_runtimeInfoManager;

    QScopedPointer<QnModuleFinder> m_moduleFinder;
	QScopedPointer<QnRouter> m_router;
	QScopedPointer<QnGlobalModuleFinder> m_globalModuleFinder;
	QScopedPointer<QnServerInterfaceWatcher> m_serverInterfaceWatcher;

    QScopedPointer<QnResourcePropertyDictionary> m_dictionary;
    QScopedPointer<QnResourceStatusDictionary> m_statusDictionary;

    QScopedPointer<QnCameraUserAttributePool> m_cameraUserAttributePool;
	QScopedPointer<QnMediaServerUserAttributesPool> m_mediaServerUserAttributesPool;

	QScopedPointer<TimerManager> m_timerManager;

    QnMainWindow *m_mainWindow;
};

#endif // CLIENT_AXHDWITNESS_H_