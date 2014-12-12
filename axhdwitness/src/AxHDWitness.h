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

    void addToCurrentLayout(const QString &uniqueId);
    void removeFromCurrentLayout(const QString &uniqueId);

    void reconnect(const QString &url);

    void maximizeItem(const QString &uniqueId);
    void unmaximizeItem(const QString &uniqueId);

    void slidePanelsOut();

    QString resourceListXml();

protected:
    virtual bool event(QEvent *event) override;

private:
    bool doInitialize();
    void doFinalize();
    void createMainWindow();

private:
    bool m_isInitialized;
    QnClientResourceProcessor m_clientResourceProcessor;

    QScopedPointer<QnWorkbenchContext> m_context;
	QScopedPointer<QnSkin> skin;
	QScopedPointer<QnCustomizer> customizer;
	QScopedPointer<QnClientModule> m_clientModule;
    QnMainWindow *m_mainWindow;
};

#endif // CLIENT_AXHDWITNESS_H_