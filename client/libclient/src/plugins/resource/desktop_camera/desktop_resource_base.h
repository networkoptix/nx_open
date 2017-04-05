#pragma once

#include <map>

#include <core/resource/resource.h>

#include <nx/streaming/abstract_archive_resource.h>
#include <core/resource/resource_fwd.h>
#include <plugins/resource/desktop_camera/desktop_camera_connection.h>

class QnDesktopResource: public QnAbstractArchiveResource
{
    Q_OBJECT;

public:
    QnDesktopResource();
    virtual ~QnDesktopResource();

    static QnUuid getDesktopResourceUuid();

    virtual void addConnection(const QnMediaServerResourcePtr& server);
    virtual void removeConnection(const QnMediaServerResourcePtr& server);

    virtual bool isRendererSlow() const = 0;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout(
        const QnAbstractStreamDataProvider *dataProvider) const override;

protected:
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(
        Qn::ConnectionRole role) = 0;

    std::map<QnUuid, QnDesktopCameraConnectionPtr> m_connectionPool;
};

