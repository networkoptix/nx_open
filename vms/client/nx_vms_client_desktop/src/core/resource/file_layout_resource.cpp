#include "file_layout_resource.h"

#include <nx/utils/log/log.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>

QnFileLayoutResource::QnFileLayoutResource(QnCommonModule* commonModule):
    base_type(commonModule)
{
    addFlags(Qn::exported_layout);
}

QString QnFileLayoutResource::getUniqueId() const
{
    return getUrl();
}

Qn::ResourceStatus QnFileLayoutResource::getStatus() const
{
    QnMutexLocker locker(&m_mutex);
    return m_localStatus;
}

bool QnFileLayoutResource::isFile() const
{
    NX_ASSERT(!getUrl().isEmpty());
    return true;
}

void QnFileLayoutResource::setIsEncrypted(bool encrypted)
{
    QnMutexLocker locker(&m_mutex);
    m_isEncrypted = encrypted;
}

bool QnFileLayoutResource::isEncrypted() const
{
    QnMutexLocker locker(&m_mutex);
    return m_isEncrypted;
}

bool QnFileLayoutResource::usePasswordToRead(const QString& password)
{
    {
        QnMutexLocker locker(&m_mutex);
        if (m_password == password)
            return true;

        m_password = password;
    }
    emit passwordChanged(toSharedPointer(this));

    return true; //< Won't check the password here. Check before calling this function.
}

QString QnFileLayoutResource::password() const
{
    QnMutexLocker locker(&m_mutex);
    return m_password;
}

void QnFileLayoutResource::setStatus(Qn::ResourceStatus newStatus,
    Qn::StatusChangeReason reason)
{
    {
        QnMutexLocker locker(&m_mutex);

        if (m_localStatus == newStatus)
            return;

        m_localStatus = newStatus;

        NX_VERBOSE(this, "Signal status change for %1", newStatus);
    }
    emit statusChanged(toSharedPointer(this), reason);
}

void QnFileLayoutResource::setReadOnly(bool readOnly)
{
    {
        QnMutexLocker locker(&m_mutex);

        if (m_isReadOnly == readOnly)
            return;

        m_isReadOnly = readOnly;
    }
    emit readOnlyChanged(toSharedPointer(this));
}

bool QnFileLayoutResource::isReadOnly() const
{
    QnMutexLocker locker(&m_mutex);
    return m_isReadOnly;
}

void QnFileLayoutResource::updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers)
{
    base_type::updateInternal(other, notifiers);

    const auto localOther = other.dynamicCast<QnFileLayoutResource>();
    if (localOther)
    {
        m_isEncrypted = localOther->isEncrypted();
        if (m_password != localOther->password())
        {
            m_password = localOther->password();
            notifiers << [r = toSharedPointer(this)]{ emit r->passwordChanged(r); };
        }
        if (m_localStatus != localOther->getStatus())
        {
            m_localStatus = localOther->getStatus();
            notifiers <<
                [r = toSharedPointer(this)]{ emit r->statusChanged(r, Qn::StatusChangeReason::Local); };
        }
        if (m_isReadOnly != localOther->m_isReadOnly)
        {
            m_isReadOnly = localOther->m_isReadOnly;
            notifiers <<
                [r = toSharedPointer(this)]{ emit r->readOnlyChanged(r); };
        }
    }
}
