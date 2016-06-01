#ifndef QN_DESKTOP_RESOURCE_H
#define QN_DESKTOP_RESOURCE_H

#include <QtCore/QtGlobal>

#include "core/resource/resource.h"
#include "plugins/resource/archive/abstract_archive_resource.h"
#include "desktop_data_provider_wrapper.h"
#include "core/resource/resource_fwd.h"
#include "plugins/resource/desktop_camera/desktop_camera_connection.h"
#include "plugins/resource/desktop_camera/desktop_resource_base.h"

#if defined(Q_OS_WIN)

class QnDesktopDataProvider;
class QGLWidget;

class QnWinDesktopResource:
    public QnDesktopResource
{
    Q_OBJECT;
public:
    QnWinDesktopResource(QGLWidget* mainWindow = 0);
    virtual ~QnWinDesktopResource();

    virtual bool isRendererSlow() const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

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

#endif

#endif // QN_DESKTOP_RESOURCE_H
