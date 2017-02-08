#pragma once

#include <core/resource/resource.h>
#include <nx/streaming/abstract_archive_resource.h>
#include <core/resource/resource_fwd.h>
#include <plugins/resource/desktop_camera/desktop_data_provider_wrapper.h>
#include <plugins/resource/desktop_camera/desktop_camera_connection.h>
#include <plugins/resource/desktop_camera/desktop_resource_base.h>

class QnDesktopDataProvider;
class QGLWidget;

class QnWinDesktopResource: public QnDesktopResource
{
public:
    QnWinDesktopResource(QGLWidget* mainWindow = 0);
    virtual ~QnWinDesktopResource();

    virtual bool isRendererSlow() const override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(const QnAbstractStreamDataProvider* /*dataProvider*/) const override;

    virtual QString toSearchString() const override;

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
