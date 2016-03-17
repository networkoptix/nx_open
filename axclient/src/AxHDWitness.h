#pragma once

#include "version.h"

#include <QtCore/QScopedPointer>

#include <ActiveQt/QAxBindable>

class QnAxClientModule;
class QnAxClientWindow;

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
    void connectionProcessed(int status, const QString &message);

protected:
    virtual bool event(QEvent *event) override;

private:
    bool doInitialize();
    void doFinalize();

private:
    bool m_isInitialized;
    QScopedPointer<QnAxClientModule> m_axClientModule;
    QScopedPointer<QnAxClientWindow> m_axClientWindow;
};
