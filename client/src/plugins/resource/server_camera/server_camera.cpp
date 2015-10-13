#include "server_camera.h"

#include <api/app_server_connection.h>

#include <plugins/resource/archive/archive_stream_reader.h>
#include <plugins/resource/archive/rtsp_client_archive_delegate.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/status_dictionary.h>

#include <core/resource/media_server_resource.h>
#include <core/resource/camera_user_attribute_pool.h>

QnServerCamera::QnServerCamera(const QnUuid& resourceTypeId): QnVirtualCameraResource()
{
    setTypeId(resourceTypeId);
    addFlags(Qn::server_live_cam | Qn::depend_on_parent_status);
}

QString QnServerCamera::getDriverName() const
{
    return QLatin1String("Server camera"); //all other manufacture are also untranslated and should not be translated
}


QString QnServerCamera::getName() const {
    return getUserDefinedName();
}

void QnServerCamera::setName(const QString& name) {
    if (getId().isNull() )
        return;

    {
        QnCameraUserAttributePool::ScopedLock userAttributesLock( QnCameraUserAttributePool::instance(), getId() );
        if ((*userAttributesLock)->name == name)
            return;
        (*userAttributesLock)->name = name;
    }
    emit nameChanged(toSharedPointer(this));
}


Qn::ResourceFlags QnServerCamera::flags() const {
    Qn::ResourceFlags result = base_type::flags();
    if (!isDtsBased() && supportedMotionType() != Qn::MT_NoMotion)
        result |= Qn::motion;
    if (isIOModule())
        result |= Qn::io_module;
    return result;
}

void QnServerCamera::setIframeDistance(int frames, int timems)
{
    Q_UNUSED(frames)
    Q_UNUSED(timems)
}

QnConstResourceVideoLayoutPtr QnServerCamera::getVideoLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    return QnVirtualCameraResource::getVideoLayout(dataProvider);
}

QnConstResourceAudioLayoutPtr QnServerCamera::getAudioLayout(const QnAbstractStreamDataProvider* dataProvider) const
{
    const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
    if (archive)
        return archive->getDPAudioLayout();
    else
        return QnMediaResource::getAudioLayout();
}


QnAbstractStreamDataProvider* QnServerCamera::createLiveDataProvider()
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnRtspClientArchiveDelegate(result));
    return result;
}

Qn::ResourceStatus QnServerCamera::getStatus() const
{
    Qn::ResourceStatus serverStatus = qnStatusDictionary->value(getParentId());
    if (serverStatus == Qn::Offline || serverStatus == Qn::Unauthorized)
        return Qn::Offline;
    
    return QnResource::getStatus();
}

void QnServerCamera::setParentId(const QnUuid& parent)
{
    QnUuid oldValue = getParentId();
    if (oldValue != parent) {
        base_type::setParentId(parent);
        if (!oldValue.isNull())
            emit statusChanged(toSharedPointer(this));
    }
}

void QnServerCamera::updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields) 
{
    if (other->getParentId() != m_parentId)
        modifiedFields << "statusChanged";
    QnVirtualCameraResource::updateInner(other, modifiedFields);
}

