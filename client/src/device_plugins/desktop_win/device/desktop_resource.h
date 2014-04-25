#ifndef QN_DESKTOP_RESOURCE_H
#define QN_DESKTOP_RESOURCE_H

#include <QtCore/QtGlobal>

#include "core/resource/resource.h"
#include "plugins/resources/archive/abstract_archive_resource.h"
#include "../desktop_data_provider_wrapper.h"
#include "core/resource/resource_fwd.h"
#include "device_plugins/desktop_camera/desktop_camera_connection.h"

#ifdef _WIN32

class QnDesktopDataProvider;

class QnDesktopResource: public QnAbstractArchiveResource {
    Q_OBJECT;
public:
    QnDesktopResource(QGLWidget* mainWindow = 0);
    virtual ~QnDesktopResource();

    virtual QString toString() const override;

    void addConnection(QnMediaServerResourcePtr mServer);
    void removeConnection(QnMediaServerResourcePtr mServer);

    QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/);

    static QUuid getDesktopResourceUuid();
protected:
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(ConnectionRole role) override;
private:
    void createSharedDataProvider();

    friend class QnDesktopDataProviderWrapper;

private:
    QGLWidget* m_mainWidget;
    QnDesktopDataProvider* m_desktopDataProvider;
    QMutex m_dpMutex;
    QMap<QUuid, QnDesktopCameraConnectionPtr> m_connectionPool;
};

#else

/*!
    This declaration is used on non-win32 platforms.
    TODO do this with platform-dependent implementation files
*/
class QnDesktopResource: public QnAbstractArchiveResource {
public:
    QnDesktopResource(QGLWidget* mainWindow = 0):
        QnAbstractArchiveResource() {Q_UNUSED(mainWindow)}
};

#endif

#endif // QN_DESKTOP_RESOURCE_H
