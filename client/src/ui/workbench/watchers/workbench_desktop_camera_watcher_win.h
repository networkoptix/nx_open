#ifndef QN_WORKBENCH_DESKTOP_CAMERA_WATCHER_H
#define QN_WORKBENCH_DESKTOP_CAMERA_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

/** Module maintaining connection between desktop resource and a server we are currently connected to. */
class QnWorkbenchDesktopCameraWatcher: public QObject, public QnWorkbenchStateDelegate {
    Q_OBJECT
    typedef QObject base_type;
public:
    QnWorkbenchDesktopCameraWatcher(QObject *parent);
    virtual ~QnWorkbenchDesktopCameraWatcher();

    /** Handle disconnect from server. */
    virtual bool tryClose(bool force) override;

    virtual void forcedUpdate() override;

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    /** Setup connection (if available). */
    void initialize();

    /** Drop connection. */
    void deinitialize();

    /** Update current server. */
    void setServer(const QnMediaServerResourcePtr &server);

private:
    QnMediaServerResourcePtr m_server;
    QnDesktopResourcePtr m_desktop;
};

#endif // QN_WORKBENCH_DESKTOP_CAMERA_WATCHER_H
