// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource.h"

#include <typeinfo>

#include <QtCore/QMetaObject>

#include <core/resource_management/resource_management_ini.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <core/resource/camera_advanced_param.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <utils/common/util.h>

using namespace nx::vms::common;

namespace {

QString hidePasswordIfCredentialsPropety(const QString& key, const QString& value)
{
    if (nx::utils::log::showPasswords())
        return value;

    if (key == ResourcePropertyKey::kCredentials
        || key == ResourcePropertyKey::kDefaultCredentials)
    {
        return value.left(value.indexOf(':')) + ":******";
    }
    else if (key == SystemSettings::Names::smtpPassword)
    {
        return "******";
    }

    return value;
}

} // namespace

// -------------------------------------------------------------------------- //
// QnResource
// -------------------------------------------------------------------------- //
QnResource::QnResource():
    m_mutex(nx::Mutex::Recursive)
{
}

QnResource::~QnResource()
{
}

QnResourcePool* QnResource::resourcePool() const
{
    if (auto context = systemContext())
        return context->resourcePool();

    return nullptr;
}

void QnResource::addToSystemContext(nx::vms::common::SystemContext* systemContext)
{
    if (!NX_ASSERT(systemContext, "Context must exist here"))
        return;

    if (!NX_ASSERT(!this->systemContext(), "Resource already belongs to some System Context"))
        return;

    setSystemContext(systemContext);
}

QnResourcePtr QnResource::toSharedPointer() const
{
    return QnFromThisToShared<QnResource>::toSharedPointer();
}

void QnResource::forceUsingLocalProperties()
{
    m_forceUseLocalProperties = true;
}

bool QnResource::useLocalProperties() const
{
    return m_forceUseLocalProperties || m_id.isNull();
}

void QnResource::updateInternal(const QnResourcePtr& source, NotifierList& notifiers)
{
    NX_ASSERT(getId() == source->getId() || getId().isNull());
    NX_ASSERT(toSharedPointer(this));

    m_typeId = source->m_typeId;

    if (m_url != source->m_url)
    {
        m_url = source->m_url;
        notifiers << [r = toSharedPointer(this)] { emit r->urlChanged(r); };
    }

    if (m_flags != source->m_flags)
    {
        m_flags = source->m_flags;
        notifiers << [r = toSharedPointer(this)] { emit r->flagsChanged(r); };
    }

    if (m_name != source->m_name)
    {
        m_name = source->m_name;
        notifiers << [r = toSharedPointer(this)] { emit r->nameChanged(r); };
    }

    if (m_parentId != source->m_parentId)
    {
        const auto previousParentId = m_parentId;
        m_parentId = source->m_parentId;
        notifiers <<
            [r = toSharedPointer(this), previousParentId]
            {
                emit r->parentIdChanged(r, previousParentId);
            };
    }

    if (useLocalProperties())
    {
        for (const auto& p: source->getProperties())
            m_locallySavedProperties.emplace(p.name, p.value);
    }
}

void QnResource::update(const QnResourcePtr& source)
{
    NotifierList notifiers;
    {
        // Maintain mutex lock order.
        nx::Mutex *m1 = &m_mutex, *m2 = &source->m_mutex;
        if (m1 > m2)
            std::swap(m1, m2);
        NX_MUTEX_LOCKER mutexLocker1(m1);
        NX_MUTEX_LOCKER mutexLocker2(m2);
        updateInternal(source, notifiers);
    }

    for (auto notifier: notifiers)
        notifier();
}

QnUuid QnResource::getParentId() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(const QnUuid& parent)
{
    QnUuid previousParentId;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_parentId == parent)
            return;

        previousParentId = m_parentId;
        m_parentId = parent;
    }

    emit parentIdChanged(toSharedPointer(this), previousParentId);
}

QString QnResource::getName() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);

        if (m_name == name)
            return;

        m_name = name;
    }

    emit nameChanged(toSharedPointer(this));
}

Qn::ResourceFlags QnResource::flags() const
{
    // A mutex is not needed, the value is atomically read.
    return m_flags;
}

void QnResource::setFlags(Qn::ResourceFlags flags)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::addFlags(Qn::ResourceFlags flags)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        flags |= m_flags;
        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::removeFlags(Qn::ResourceFlags flags)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        flags = m_flags & ~flags;
        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

QnResourcePtr QnResource::getParentResource() const
{
    if (const auto resourcePool = this->resourcePool())
        return resourcePool->getResourceById(getParentId());

    return QnResourcePtr();
}

QnUuid QnResource::getTypeId() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const QnUuid& id)
{
    if (!NX_ASSERT(!id.isNull()))
        return;

    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    m_typeId = id;
}

nx::vms::api::ResourceStatus QnResource::getStatus() const
{
    if (auto context = systemContext())
    {
        const auto statusDictionary = context->resourceStatusDictionary();
        if (statusDictionary)
            return statusDictionary->value(getId());
    }
    return ResourceStatus::undefined;
}

nx::vms::api::ResourceStatus QnResource::getPreviousStatus() const
{
    return m_previousStatus;
}

void QnResource::setStatus(ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    if (newStatus == ResourceStatus::undefined)
    {
        NX_VERBOSE(this, "Won't change status of resource %1 (%2) because it is 'undefined'",
            getId(), nx::utils::url::hidePassword(getUrl()));
        return;
    }

    if (hasFlags(Qn::removed))
    {
        NX_VERBOSE(this, "Won't change status of resource %1 (%2) because it has a 'removed' flag",
            getId(), nx::utils::url::hidePassword(getUrl()));
        return;
    }

    auto context = systemContext();
    if (!NX_ASSERT(context))
        return;

    QnUuid id = getId();
    ResourceStatus oldStatus = context->resourceStatusDictionary()->value(id);
    if (oldStatus == newStatus)
    {
        NX_VERBOSE(this, "Won't change status of resource %1 (%2) because status (%3) hasn't changed",
            getId(), nx::utils::url::hidePassword(getUrl()), newStatus);
        return;
    }

    NX_DEBUG(this,
        "Status changed %1 -> %2, reason=%3, name=[%4], url=[%5]",
        oldStatus,
        newStatus,
        reason,
        getName(),
        nx::utils::url::hidePassword(getUrl()));
    m_previousStatus = oldStatus;
    context->resourceStatusDictionary()->setValue(id, newStatus);

    // Null pointer if we are changing status in constructor. Signal is not needed in this case.
    if (auto sharedThis = toSharedPointer(this))
    {
        NX_VERBOSE(this, "Signal status change for %1", newStatus);
        emit statusChanged(sharedThis, reason);
    }
}

void QnResource::setIdUnsafe(const QnUuid& id)
{
    m_id = id;
}

QString QnResource::getUrl() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString& url)
{
    {
        NX_MUTEX_LOCKER mutexLocker(&m_mutex);
        if (!setUrlUnsafe(url))
            return;
    }

    emit urlChanged(toSharedPointer(this));
}

int QnResource::logicalId() const
{
    return 0;
}

void QnResource::setLogicalId(int /*value*/)
{
    // Base implementation does not keep logical Id.
}

QString QnResource::getProperty(const QString& key) const
{
    QString value;
    {
        if (useLocalProperties())
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            auto itr = m_locallySavedProperties.find(key);
            if (itr != m_locallySavedProperties.end())
                value = itr->second;
        }
        else if (auto context = systemContext(); context && context->resourcePropertyDictionary())
        {
            value = context->resourcePropertyDictionary()->value(m_id, key);
        }
    }

    if (value.isNull())
    {
        // Find the default value in the Resource type.
        if (QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId()))
            return resType->defaultValue(key);
    }
    return value;
}

bool QnResource::setProperty(const QString& key, const QString& value, bool markDirty)
{
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (useLocalProperties())
        {
            auto itr = m_locallySavedProperties.find(key);
            if (itr == m_locallySavedProperties.end())
            {
                m_locallySavedProperties.emplace(key, value);
                return true;
            }
            if (value == itr->second)
                return false;
            itr->second = value;
            return true;
        }
    }

    NX_ASSERT(!getId().isNull());
    if (const auto context = systemContext(); NX_ASSERT(context))
    {
        auto prevValue = getProperty(key);
        const bool isModified = context->resourcePropertyDictionary()->setValue(
            getId(),
            key,
            value,
            markDirty);

        if (isModified)
            emitPropertyChanged(key, prevValue, value);

        return isModified;
    }

    return false;
}

void QnResource::emitPropertyChanged(
    const QString& key, const QString& prevValue, const QString& newValue)
{
    if (key == ResourcePropertyKey::kVideoLayout)
        emit videoLayoutChanged(::toSharedPointer(this));

    NX_VERBOSE(this,
        "Changed property '%1' = '%2'",
        key,
        hidePasswordIfCredentialsPropety(key, getProperty(key)));
    emit propertyChanged(toSharedPointer(this), key, prevValue, newValue);
}

bool QnResource::setUrlUnsafe(const QString& value)
{
    if (m_url == value)
        return false;

    m_url = value;
    return true;
}

nx::vms::api::ResourceParamDataList QnResource::getProperties() const
{
    if (useLocalProperties())
    {
        nx::vms::api::ResourceParamDataList result;
        for (const auto& prop: m_locallySavedProperties)
            result.emplace_back(prop.first, prop.second);
        return result;
    }

    if (const auto context = systemContext())
        return context->resourcePropertyDictionary()->allProperties(getId());

    return {};
}

bool QnResource::saveProperties()
{
    NX_ASSERT(systemContext() && !getId().isNull());
    if (auto context = systemContext())
        return context->resourcePropertyDictionary()->saveParams(getId());
    return false;
}

void QnResource::savePropertiesAsync()
{
    NX_ASSERT(systemContext() && !getId().isNull());
    if (auto context = systemContext())
        context->resourcePropertyDictionary()->saveParamsAsync(getId());
}

// -----------------------------------------------------------------------------

void QnResource::setSystemContext(nx::vms::common::SystemContext* systemContext)
{
    m_systemContext = systemContext;
}

nx::vms::common::SystemContext* QnResource::systemContext() const
{
    if (auto systemContext = m_systemContext.load())
        return systemContext;

    return nullptr;
}

QString QnResource::idForToStringFromPtr() const
{
    return NX_FMT("%1: %2", getId().toSimpleString(), getName());
}
