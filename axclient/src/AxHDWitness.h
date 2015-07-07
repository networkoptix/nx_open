#ifndef CLIENT_AXHDWITNESS_H_
#define CLIENT_AXHDWITNESS_H_

#include "version.h"

#include <QtCore/QScopedPointer>

#include <ActiveQt/QAxBindable>

#include "ui/widgets/main_window.h"
#include "ui/style/skin.h"
#include <client/client_module.h>
#include "ui/style/noptix_style.h"
#include "ui/customization/customizer.h"
#include "nx_ec/ec_api.h"
#include "api/abstract_connection.h"

class QnModuleFinder;
class QnRouter;
class QnGlobalModuleFinder;
class QnServerInterfaceWatcher;

class AxHDWitness : public QWidget
{
    Q_OBJECT

    Q_CLASSINFO("ClassID", QN_AX_CLASS_ID);
    Q_CLASSINFO("InterfaceID", QN_AX_INTERFACE_ID);
    Q_CLASSINFO("EventsID", QN_AX_EVENTS_ID);

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

    double minimalSpeed();
    double maximalSpeed();
    double speed();
    void setSpeed(double speed);

    qint64 currentTime();
    void setCurrentTime(qint64 time);

    void addResourceToLayout(const QString &uniqueId, const QString &timestamp);
    void addResourcesToLayout(const QString &uniqueIds, const QString &timestamp);
    void removeFromCurrentLayout(const QString &uniqueId);

    void reconnect(const QString &url);

    void maximizeItem(const QString &uniqueId);
    void unmaximizeItem(const QString &uniqueId);

    void slidePanelsOut();

    QString resourceListXml();

signals:
//     void started();
//     void paused();
//     void speedChanged(double speed);

    void connectionProcessed(int status, const QString &message);

protected:
    virtual bool event(QEvent *event) override;

private:
    bool doInitialize();
    void doFinalize();
    void createMainWindow();

private:
    bool m_isInitialized;

    QScopedPointer<ec2::AbstractECConnectionFactory> m_ec2ConnectionFactory;

    QScopedPointer<QnWorkbenchContext> m_context;
    QScopedPointer<QnSkin> skin;
    QScopedPointer<QnCustomizer> customizer;
    QScopedPointer<QnClientModule> m_clientModule;

    QScopedPointer<QnModuleFinder> m_moduleFinder;
    QScopedPointer<QnRouter> m_router;
    QScopedPointer<QnServerInterfaceWatcher> m_serverInterfaceWatcher;


    QnMainWindow *m_mainWindow;
};

#endif // CLIENT_AXHDWITNESS_H_