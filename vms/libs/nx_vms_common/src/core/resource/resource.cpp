// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource.h"
#include "resource_consumer.h"

#include <typeinfo>

#include <QtCore/QMetaObject>
#include <QtCore/QRunnable>
#include <QtCore/QThreadPool>

#include <common/common_module.h>
#include <core/resource/camera_advanced_param.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/status_dictionary.h>
#include <utils/common/util.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/metrics/metrics_storage.h>

#include <core/resource_management/resource_management_ini.h>
#include <api/global_settings.h>

namespace {

} // namespace

// -------------------------------------------------------------------------- //
// QnResource
// -------------------------------------------------------------------------- //
QnResource::QnResource(QnCommonModule* commonModule):
    m_mutex(nx::Mutex::Recursive),
    m_commonModule(commonModule)
{
}

QnResource::QnResource(const QnResource& right):
    QObject(),
    m_parentId(right.m_parentId),
    m_name(right.m_name),
    m_url(right.m_url),
    m_resourcePool(right.m_resourcePool.load()),
    m_id(right.m_id),
    m_typeId(right.m_typeId),
    m_flags(right.m_flags),
    m_prevInitializationResult(right.m_prevInitializationResult),
    m_locallySavedProperties(right.m_locallySavedProperties),
    m_initState(right.m_initState.load()),
    m_commonModule(right.m_commonModule.load())
{
}

QnResource::~QnResource()
{
    disconnectAllConsumers();
}

QnResourcePool *QnResource::resourcePool() const
{
    return m_resourcePool;
}

void QnResource::setResourcePool(QnResourcePool *resourcePool)
{
    m_resourcePool = resourcePool;
}

QnResourcePtr QnResource::toSharedPointer() const
{
    return QnFromThisToShared<QnResource>::toSharedPointer();
}

bool QnResource::emitDynamicSignal(const char *signal, void **arguments)
{
    QByteArray theSignal = QMetaObject::normalizedSignature(signal);
    int signalId = metaObject()->indexOfSignal(theSignal);
    if (signalId == -1)
        return false;
    metaObject()->activate(this, signalId, arguments);
    return true;
}

void QnResource::setForceUseLocalProperties(bool value)
{
    m_forceUseLocalProperties = value;
}

bool QnResource::useLocalProperties() const
{
    return m_forceUseLocalProperties || m_id.isNull();
}

void QnResource::updateInternal(const QnResourcePtr& other, Qn::NotifierList& notifiers)
{
    // unique id MUST be the same
    NX_ASSERT(getId() == other->getId() || getUniqueId() == other->getUniqueId());
    NX_ASSERT(toSharedPointer(this));

    m_typeId = other->m_typeId;

    if (m_url != other->m_url)
    {
        m_url = other->m_url;
        notifiers << [r = toSharedPointer(this)]{ emit r->urlChanged(r); };
    }

    if (m_flags != other->m_flags)
    {
        m_flags = other->m_flags;
        notifiers << [r = toSharedPointer(this)]{ emit r->flagsChanged(r); };
    }

    if (m_name != other->m_name)
    {
        m_name = other->m_name;
        notifiers << [r = toSharedPointer(this)]{emit r->nameChanged(r);};
    }

    if (m_parentId != other->m_parentId)
    {
        const auto previousParentId = m_parentId;
        m_parentId = other->m_parentId;
        notifiers <<
            [r = toSharedPointer(this), previousParentId]
            {
                emit r->parentIdChanged(r, previousParentId);
            };
    }

    m_locallySavedProperties = other->m_locallySavedProperties;
    if (useLocalProperties() && !other->useLocalProperties())
    {
        for (const auto& p : other->getRuntimeProperties())
            m_locallySavedProperties.emplace(p.name, LocalPropertyValue(p.value, true, true));
    }
}

void QnResource::update(const QnResourcePtr& other)
{
    Qn::NotifierList notifiers;
    {
        nx::Mutex *m1 = &m_mutex, *m2 = &other->m_mutex;
        if (m1 > m2)
            std::swap(m1, m2);  //to maintain mutex lock order
        NX_MUTEX_LOCKER mutexLocker1(m1);
        NX_MUTEX_LOCKER mutexLocker2(m2);
        updateInternal(other, notifiers);
    }

    for (auto notifier : notifiers)
        notifier();
}

QnUuid QnResource::getParentId() const
{
    NX_MUTEX_LOCKER locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(const QnUuid& parent)
{
    bool initializedChanged = false;
    QnUuid previousParentId;
    {
        NX_MUTEX_LOCKER locker(&m_mutex);
        if (m_parentId == parent)
            return;

        previousParentId = m_parentId;
        m_parentId = parent;
    }

    emit parentIdChanged(toSharedPointer(this), previousParentId);

    if (initializedChanged)
        emit this->initializedChanged(toSharedPointer(this));
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
    //NX_MUTEX_LOCKER mutexLocker( &m_mutex );
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

bool QnResource::hasDefaultProperty(const QString &name) const
{
    QnResourceTypePtr resType = qnResTypePool->getResourceType(m_typeId);
    if (!resType)
        return false;
    return resType->hasParam(name);
}

QnUuid QnResource::getTypeId() const
{
    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const QnUuid &id)
{
    if (id.isNull())
    {
        qWarning() << "NULL typeId is set to resource" << getName();
        return;
    }

    NX_MUTEX_LOCKER mutexLocker(&m_mutex);
    m_typeId = id;
}

void QnResource::setTypeByName(const QString& resTypeName)
{
    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(resTypeName);
    if (resType)
        setTypeId(resType->getId());
}

nx::vms::api::ResourceStatus QnResource::getStatus() const
{
    if (auto commonModule = this->commonModule())
    {
        const auto statusDictionary = commonModule->resourceStatusDictionary();
        if (statusDictionary)
            return statusDictionary->value(getId());
    }
    return nx::vms::api::ResourceStatus::undefined;
}

nx::vms::api::ResourceStatus QnResource::getPreviousStatus() const
{
    return m_previousStatus;
}

void QnResource::setStatus(nx::vms::api::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    if (newStatus == nx::vms::api::ResourceStatus::undefined)
        return;

    if (hasFlags(Qn::removed))
        return;

    if (!commonModule())
        return;

    QnUuid id = getId();
    nx::vms::api::ResourceStatus oldStatus = commonModule()->resourceStatusDictionary()->value(id);
    if (oldStatus == newStatus)
        return;

    NX_DEBUG(this, "Status changed %1 -> %2, reason=%3, name=[%4], url=[%5]",
        oldStatus, newStatus, reason, getName(), nx::utils::url::hidePassword(getUrl()));
    m_previousStatus = oldStatus;
    commonModule()->resourceStatusDictionary()->setValue(id, newStatus);
    if ((oldStatus != nx::vms::api::ResourceStatus::undefined)
        && (oldStatus != nx::vms::api::ResourceStatus::mismatchedCertificate)
        && (newStatus == nx::vms::api::ResourceStatus::offline))
    {
        commonModule()->metrics()->offlineStatus()++;
    }

    if ((newStatus == nx::vms::api::ResourceStatus::offline)
        || (newStatus == nx::vms::api::ResourceStatus::unauthorized)
        || (newStatus == nx::vms::api::ResourceStatus::mismatchedCertificate))
    {
        if (switchState(initDone, initNone))
        {
            NX_VERBOSE(this,
                "Mark resource %1 as uninitialized because its status %2", getId(), newStatus);
            emit initializedChanged(toSharedPointer(this));
        }
    }

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

void QnResource::addConsumer(QnResourceConsumer *consumer)
{
    NX_MUTEX_LOCKER locker(&m_consumersMtx);

    if (m_consumers.contains(consumer))
    {
        NX_ASSERT(false, "Given resource consumer '%1' is already associated with this resource.", typeid(*consumer).name());
        return;
    }

    m_consumers.insert(consumer);
}

void QnResource::removeConsumer(QnResourceConsumer *consumer)
{
    NX_MUTEX_LOCKER locker(&m_consumersMtx);

    m_consumers.remove(consumer);
}

bool QnResource::hasConsumer(QnResourceConsumer *consumer) const
{
    NX_MUTEX_LOCKER locker(&m_consumersMtx);
    return m_consumers.contains(consumer);
}

void QnResource::disconnectAllConsumers()
{
    NX_MUTEX_LOCKER locker(&m_consumersMtx);

    for (QnResourceConsumer *consumer : m_consumers)
        consumer->beforeDisconnectFromResource();

    for (QnResourceConsumer *consumer : m_consumers)
        consumer->disconnectFromResource();

    m_consumers.clear();
}

CameraDiagnostics::Result QnResource::initInternal()
{
    return CameraDiagnostics::NoErrorResult();
}

void QnResource::initializationDone()
{
}

bool QnResource::hasProperty(const QString &key) const
{
    if (!commonModule())
        return false;

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (useLocalProperties()
            && m_locallySavedProperties.find(key) != m_locallySavedProperties.end())
        {
            return true;
        }
    }
    return commonModule()->resourcePropertyDictionary()->hasProperty(getId(), key);
}

QString QnResource::getProperty(const QString &key) const
{
    QString value;
    {
        if (useLocalProperties())
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            auto itr = m_locallySavedProperties.find(key);
            if (itr != m_locallySavedProperties.end())
                value = itr->second.value;
        }
        else if (auto module = commonModule(); module && module->resourcePropertyDictionary())
        {
            value = module->resourcePropertyDictionary()->value(m_id, key);
        }
    }

    if (value.isNull())
    {
        // find default value in resourceType
        QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId());
        if (resType)
            return resType->defaultValue(key);
    }
    return value;
}

QString QnResource::getResourceProperty(
    QnCommonModule* commonModule,
    const QString& key,
    const QnUuid &resourceId,
    const QnUuid &resourceTypeId)
{
    // TODO: #sivanov Code duplication.
    NX_ASSERT(!resourceId.isNull() && !resourceTypeId.isNull(), "Invalid input, reading from local data is required.");

    NX_ASSERT(commonModule);
    QString value = commonModule
        ? commonModule->resourcePropertyDictionary()->value(resourceId, key)
        : QString();

    if (value.isNull())
    {
        QnResourceTypePtr resType = qnResTypePool->getResourceType(resourceTypeId);
        if (resType)
            return resType->defaultValue(key);
    }
    return value;
}

bool QnResource::setProperty(const QString &key, const QString &value, PropertyOptions options)
{
    const bool markDirty = !(options & NO_MARK_DIRTY);
    const bool replaceIfExists = !(options & NO_REPLACE_IF_EXIST);

    if ((options & NO_ALLOW_EMPTY) && value.isEmpty())
        return false;

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (useLocalProperties())
        {
            // Saving property to some internal dictionary.
            // Will apply to global dictionary when id is known.
            m_locallySavedProperties[key] = LocalPropertyValue(value, markDirty, replaceIfExists);

            //calling resourcePropertyDictionary()->saveProperties(...) does not make any sense
            return false;
        }
    }

    NX_ASSERT(!getId().isNull());
    NX_ASSERT(commonModule());

    auto prevValue = getProperty(key);
    const bool isModified =
        commonModule() && commonModule()->resourcePropertyDictionary()->setValue(
            getId(), key, value, markDirty, replaceIfExists);

    if (isModified)
        emitPropertyChanged(key, prevValue, value);

    return isModified;
}

bool QnResource::setProperty(const QString &key, const QVariant& value, PropertyOptions options)
{
    return setProperty(key, value.toString(), options);
}

static QString hidePasswordIfCredentialsPropety(const QString& key, const QString& value)
{
    if (nx::utils::log::showPasswords())
        return value;

    if (key == ResourcePropertyKey::kCredentials
        || key == ResourcePropertyKey::kDefaultCredentials)
    {
        return value.left(value.indexOf(':')) + ":******";
    }
    else if (key == nx::settings_names::kNameSmtpPassword)
    {
        return "******";
    }

    return value;
}

void QnResource::emitPropertyChanged(const QString& key, const QString& prevValue, const QString& newValue)
{
    if (key == ResourcePropertyKey::kVideoLayout)
        emit videoLayoutChanged(::toSharedPointer(this));

    NX_VERBOSE(this, "Changed property '%1' = '%2'", key,
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

nx::vms::api::ResourceParamDataList QnResource::getRuntimeProperties() const
{
    if (useLocalProperties())
    {
        nx::vms::api::ResourceParamDataList result;
        for (const auto& prop: m_locallySavedProperties)
            result.emplace_back(prop.first, prop.second.value);
        return result;
    }

    if (const auto module = commonModule())
        return module->resourcePropertyDictionary()->allProperties(getId());

    return {};
}

nx::vms::api::ResourceParamDataList QnResource::getAllProperties() const
{
    nx::vms::api::ResourceParamDataList result;
    nx::vms::api::ResourceParamDataList runtimeProperties;
    if (const auto module = commonModule())
        runtimeProperties = module->resourcePropertyDictionary()->allProperties(getId());

    QnResourceType::ParamTypeMap defaultProperties;

    QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId());
    if (resType)
        defaultProperties = resType->paramTypeList();

    for (auto it = defaultProperties.cbegin(); it != defaultProperties.cend(); ++it)
    {
        auto runtimeIt = std::find_if(runtimeProperties.cbegin(), runtimeProperties.cend(),
            [it](const auto& param) { return it.key() == param.name; });
        if (runtimeIt == runtimeProperties.cend())
            result.emplace_back(it.key(), it.value());
    }

    std::copy(runtimeProperties.cbegin(), runtimeProperties.cend(), std::back_inserter(result));
    return result;
}

bool QnResource::saveProperties()
{
    NX_ASSERT(commonModule() && !getId().isNull());
    if (auto module = commonModule())
        return module->resourcePropertyDictionary()->saveParams(getId());
    return false;
}

int QnResource::savePropertiesAsync()
{
    NX_ASSERT(commonModule() && !getId().isNull());
    if (auto module = commonModule())
        return module->resourcePropertyDictionary()->saveParamsAsync(getId());
    return false;
}

void QnResource::emitModificationSignals(const QSet<QByteArray>& modifiedFields)
{
    m_tryToInitTimer.invalidate(); //< Try to init again without delay.

    emit resourceChanged(toSharedPointer(this));

    const QnResourcePtr & _t1 = toSharedPointer(this);
    void *_a[] = {0, const_cast<void*>(reinterpret_cast<const void*>(&_t1))};
    for (const QByteArray& signalName : modifiedFields)
        emitDynamicSignal((signalName + QByteArray("(QnResourcePtr)")).data(), _a);
}

// -----------------------------------------------------------------------------

bool QnResource::switchState(InitState from, InitState to)
{
    return m_initState.compare_exchange_strong(from, to);
}

bool QnResource::init()
{
    using namespace std::chrono;
    using namespace std::literals::chrono_literals;

    auto commonModule = this->commonModule();
    if (!commonModule || commonModule->isNeedToStop())
        return false;

    if (!switchState(initNone, initInProgress))
        return false; //< Already initializing

    // Prevent to call two init() at the same time. This mutex is not used any more in other places.
    // The potential race condition is possible after switchState(none) call,
    // but before change resource status.
    NX_MUTEX_LOCKER lock(&m_initMutex);

    if (isOnline())
        setStatus(nx::vms::api::ResourceStatus::offline);

    const auto parentId = getParentId();
    CameraDiagnostics::Result initResult;
    bool isInitialized = false;
    while(1)
    {
        NX_DEBUG(this, "Start initializing for resource %1", getId());
        if (parentId == getParentId())
            initResult = initInternal();
        NX_DEBUG(this, "Initialization finished for resource %1 with result: %2",
            getId(), initResult.toString(commonModule->resourcePool()));

        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            m_prevInitializationResult = initResult;
            isInitialized = (parentId == m_parentId)
                && (initResult.errorCode == CameraDiagnostics::ErrorCode::noError);
        }

        if (switchState(initInProgress, isInitialized ? initDone : initNone))
            break;
        NX_VERBOSE(this, "Reinit is requested during previous initialization for resource %1. Init again.", getId());
        NX_ASSERT(switchState(reinitRequested, initInProgress));
    }

    if (isInitialized)
    {
        setStatus(nx::vms::api::ResourceStatus::online);
        initializationDone();
        emit initializedChanged(toSharedPointer(this));
    }
    else if (isOnline())
    {
        setStatus(nx::vms::api::ResourceStatus::offline);
    }

    m_elapsedSinceLastInit.restart();

    return true;
}

class InitAsyncTask: public QRunnable
{
public:
    InitAsyncTask(QnResourcePtr resource): m_resource(resource) {}
    void run()
    {
        m_resource->init();
    }
private:
    QnResourcePtr m_resource;
};

void QnResource::reinitAsync()
{
    if (resourcePool() == nullptr || commonModule()->isNeedToStop() || hasFlags(Qn::foreigner))
        return;

    NX_DEBUG(this, "Reinitialization is requested for resource %1", getId());
    if (switchState(initInProgress, reinitRequested))
        return; //< Initialization will be requested again after current init is finished.
    if (!isInitialized() || switchState(initDone, initNone))
        initAsync(); //< Schedule init request
}

void QnResource::initAsync()
{
    const auto resourcePool = this->resourcePool();
    if (!resourcePool)
    {
        NX_DEBUG(this,
            "Not running init task for resource %1: resource pool is unavailable", getId());
        return;
    }
    else if (commonModule()->isNeedToStop())
    {
        NX_VERBOSE(this, "Not running init task for resource %1: server is stopping", getId());
        return;
    }
    else if (hasFlags(Qn::foreigner))
    {
        NX_VERBOSE(this, "Not running init task for resource %1: removed to other server", getId());
        return;
    }

    NX_VERBOSE(this, "Async init requested for resource %1)", getId());
    InitAsyncTask* task = new InitAsyncTask(toSharedPointer(this));
    resourcePool->threadPool()->start(task);
}

void QnResource::tryToInitAsync()
{
    constexpr std::chrono::seconds kCameraInitInterval{5};
    if (m_tryToInitTimer.hasExpired(kCameraInitInterval))
    {
        NX_VERBOSE(this, "Trying to init not initialized camera [%1]", this);
        m_tryToInitTimer.restart();
        initAsync();
    }
}

CameraDiagnostics::Result QnResource::prevInitializationResult() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_prevInitializationResult;
}

bool QnResource::isInitialized() const
{
    return m_initState == initDone;
}

bool QnResource::isInitializationInProgress() const
{
    const auto state = m_initState.load();
    return state == initInProgress || state == reinitRequested;
}

void QnResource::setUniqId(const QString& /*value*/)
{
    NX_ASSERT(false, "Not implemented");
}

void QnResource::setCommonModule(QnCommonModule* commonModule)
{
    m_commonModule = commonModule;
}

QnCommonModule* QnResource::commonModule() const
{
    if (auto commonModule = m_commonModule.load())
        return commonModule;

    if (const auto pool = resourcePool())
        return pool->commonModule();

    return nullptr;
}

QString QnResource::idForToStringFromPtr() const
{
    return getId().toSimpleString();
}
