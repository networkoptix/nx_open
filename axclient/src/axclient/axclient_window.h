#pragma once

#include <utils/common/singleton.h>

class QnMainWindow;
class QnWorkbenchContext;

class QnAxClientWindow: public QObject, public Singleton<QnAxClientWindow> {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnAxClientWindow(QWidget *parent = nullptr);
    virtual ~QnAxClientWindow();

    void show();

    /* Public interface methods */
    void jumpToLive();
    void play();
    void pause();
    void nextFrame();
    void prevFrame();
    void clear();

    double minimalSpeed() const;
    double maximalSpeed() const;
    double speed() const;
    void setSpeed(double speed);

    qint64 currentTimeUsec() const;
    void setCurrentTimeUsec(qint64 timeUsec);

    void addResourcesToLayout(const QStringList &uniqueIds, qint64 timeStampMs);
    void removeFromCurrentLayout(const QString &uniqueId);

    void reconnect(const QString &url);

    void maximizeItem(const QString &uniqueId);
    void unmaximizeItem(const QString &uniqueId);

    void slidePanelsOut();        

signals:
    void connected();
    void disconnected();

private:
    void createMainWindow();

private:
    QWidget *m_parentWidget;
    QScopedPointer<QnMainWindow> m_mainWindow;
    QScopedPointer<QnWorkbenchContext> m_context;
};

#define qnAxClient QnAxClientWindow::instance()
