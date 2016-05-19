#ifndef QN_DESKTOP_RESOURCE_H
#define QN_DESKTOP_RESOURCE_H

#include <QtCore/QtGlobal>

#include <core/resource/resource.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <plugins/resource/desktop_win/desktop_data_provider_wrapper.h>
#include <core/resource/resource_fwd.h>
#include <plugins/resource/desktop_camera/desktop_camera_connection.h>

#ifdef _WIN32

class QnDesktopDataProvider;

class QnDesktopResource: public QnAbstractArchiveResource {
    Q_OBJECT;
public:
    QnDesktopResource(QGLWidget* mainWindow = 0);
    virtual ~QnDesktopResource();

    virtual QString toString() const override;
    bool isRendererSlow() const;

    void addConnection(const QnMediaServerResourcePtr &server);
    void removeConnection(const QnMediaServerResourcePtr &server);

    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    static QnUuid getDesktopResourceUuid();
    bool isConnectedTo(const QnMediaServerResourcePtr &server) const;
protected:
    virtual QnAbstractStreamDataProvider *createDataProviderInternal(Qn::ConnectionRole role) override;
private:
    void createSharedDataProvider();

    friend class QnDesktopDataProviderWrapper;

private:
    QGLWidget* m_mainWidget;
    QnDesktopDataProvider* m_desktopDataProvider;
    QnMutex m_dpMutex;
    QMap<QnUuid, QnDesktopCameraConnectionPtr> m_connectionPool;
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
