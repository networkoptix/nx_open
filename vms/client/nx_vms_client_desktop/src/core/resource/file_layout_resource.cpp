// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_layout_resource.h"

#include <nx/utils/log/log.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_resource.h>

#include <client/client_globals.h>

using namespace nx::vms::client::desktop;

QnFileLayoutResource::QnFileLayoutResource(const nx::vms::client::desktop::NovMetadata& metadata):
    m_metadata(metadata)
{
    addFlags(Qn::exported_layout);
}

nx::vms::api::ResourceStatus QnFileLayoutResource::getStatus() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_localStatus;
}

bool QnFileLayoutResource::isFile() const
{
    NX_ASSERT(!getUrl().isEmpty());
    return true;
}

void QnFileLayoutResource::setIsEncrypted(bool encrypted)
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    m_isEncrypted = encrypted;
}

bool QnFileLayoutResource::isEncrypted() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_isEncrypted;
}

bool QnFileLayoutResource::usePasswordToRead(const QString& password)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_password == password)
            return true;

        m_password = password;
    }
    emit passwordChanged(toSharedPointer(this));

    return true; //< Won't check the password here. Check before calling this function.
}

QString QnFileLayoutResource::password() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_password;
}

NovMetadata QnFileLayoutResource::metadata() const
{
    return m_metadata;
}

void QnFileLayoutResource::setStatus(nx::vms::api::ResourceStatus newStatus,
    Qn::StatusChangeReason reason)
{
    {
        NX_MUTEX_LOCKER locker(&m_mutex);

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
        NX_MUTEX_LOCKER locker(&m_mutex);

        if (m_isReadOnly == readOnly)
            return;

        m_isReadOnly = readOnly;
    }
    emit readOnlyChanged(toSharedPointer(this));
}

bool QnFileLayoutResource::isReadOnly() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_isReadOnly;
}

void QnFileLayoutResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    base_type::updateInternal(source, notifiers);

    const auto localOther = source.dynamicCast<QnFileLayoutResource>();
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

        // TODO: #sivanov Temporary solution. Must be converted to a field like everything else.
        setData(Qn::LayoutWatermarkRole, localOther->data(Qn::LayoutWatermarkRole));
    }
}
