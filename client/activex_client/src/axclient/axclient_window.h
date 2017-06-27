#pragma once

#include <client_core/connection_context_aware.h>

#include <nx/utils/singleton.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class MainWindow;

namespace workbench {

} // namespace workbench
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

class QnWorkbenchContext;
class QnWorkbenchAccessController;

class QnAxClientWindow: public QObject,
    public Singleton<QnAxClientWindow>,
    public QnConnectionContextAware
{
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

    void addResourcesToLayout(const QList<QnUuid>& uniqueIds, qint64 timeStampMs);
    void removeFromCurrentLayout(const QnUuid& uniqueId);

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
    QScopedPointer<QnWorkbenchContext> m_context;
    QScopedPointer<QnWorkbenchAccessController> m_accessController;
    QScopedPointer<nx::client::desktop::ui::MainWindow> m_mainWindow;
};

#define qnAxClient QnAxClientWindow::instance()
