#ifndef QN_DESKTOP_RESOURCE_H
#define QN_DESKTOP_RESOURCE_H

#include "core/resource/resource.h"
#include "plugins/resources/archive/abstract_archive_resource.h"
#include "../desktop_data_provider_wrapper.h"
#include "core/resource/resource_fwd.h"
#include "device_plugins/desktop_camera/desktop_camera_connection.h"

class QnDesktopDataProvider;

class QnDesktopResource: public QnAbstractArchiveResource {
    Q_OBJECT;
public:
    QnDesktopResource(QGLWidget* mainWindow = 0);
    virtual ~QnDesktopResource();

    virtual QString toString() const override;
    bool isRendererSlow() const;

    void addConnection(QnMediaServerResourcePtr mServer);
    void removeConnection(QnMediaServerResourcePtr mServer);
protected:
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(ConnectionRole role) override;
private:
    void createSharedDataProvider();

    friend class QnDesktopDataProviderWrapper;

private:
    QGLWidget* m_mainWidget;
    QnDesktopDataProvider* m_desktopDataProvider;
    QMutex m_dpMutex;
    QMap<QString, QnDesktopCameraConnectionPtr> m_connectionPool;
};

#endif // QN_DESKTOP_RESOURCE_H
