#include "resource.h"

#include <climits>

#include <typeinfo>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QMetaProperty>
#include <QtCore/QRunnable>

#include <nx_ec/data/api_resource_data.h>
#include <nx/fusion/model_functions.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/utils/log/log.h>

#include <core/resource/camera_advanced_param.h>
#include <utils/common/warnings.h>
#include "core/resource_management/resource_pool.h"
#include "core/ptz/abstract_ptz_controller.h"

#include "resource_command_processor.h"
#include "resource_consumer.h"
#include "resource_property.h"

#include "utils/common/synctime.h"
#include "utils/common/util.h"
#include "resource_command.h"
#include "../resource_management/resource_properties.h"
#include "../resource_management/status_dictionary.h"

#include <core/resource/security_cam_resource.h>
#include <common/common_module.h>

#include <nx/utils/log/assert.h>

std::atomic<bool> QnResource::m_appStopping(false);
QnMutex QnResource::m_initAsyncMutex;

// TODO: #rvasilenko move it to QnResourcePool
Q_GLOBAL_STATIC(QnInitResPool, initResPool)

static const qint64 MIN_INIT_INTERVAL = 1000000ll * 30;


// -------------------------------------------------------------------------- //
// QnResourceGetParamCommand
// -------------------------------------------------------------------------- //
#ifdef ENABLE_DATA_PROVIDERS
class QnResourceGetParamCommand: public QnResourceCommand
{
public:
    QnResourceGetParamCommand(const QnResourcePtr &res, const QString& id):
        QnResourceCommand(res),
        m_id(id)
    {
    }

    virtual void beforeDisconnectFromResource() {}

    bool execute()
    {
        bool success = false;
        QString value;
        if (isConnectedToTheResource())
        {
            success = m_resource->getParamPhysical(m_id, value);
        }
        emit m_resource->asyncParamGetDone(m_resource, m_id, value, success);
        return success;
    }

private:
    QString m_id;
};

typedef std::shared_ptr<QnResourceGetParamCommand> QnResourceGetParamCommandPtr;

// -------------------------------------------------------------------------- //
// QnResourceGetParamsCommand
// -------------------------------------------------------------------------- //
class QnResourceGetParamsCommand: public QnResourceCommand
{
public:
    QnResourceGetParamsCommand(const QnResourcePtr &res, const QSet<QString>& ids):
        QnResourceCommand(res),
        m_ids(ids)
    {
    }

    virtual void beforeDisconnectFromResource() {}

    bool execute()
    {
        bool success = isConnectedToTheResource();
        QnCameraAdvancedParamValueList result;
        if (success)
            success = m_resource->getParamsPhysical(m_ids, result);

        emit m_resource->asyncParamsGetDone(m_resource, result);
        return success;
    }

private:
    QSet<QString> m_ids;
};

typedef std::shared_ptr<QnResourceGetParamsCommand> QnResourceGetParamsCommandPtr;

// -------------------------------------------------------------------------- //
// QnResourceSetParamCommand
// -------------------------------------------------------------------------- //
class QnResourceSetParamCommand: public QnResourceCommand
{
public:
    QnResourceSetParamCommand(const QnResourcePtr &res, const QString& id, const QString& value):
        QnResourceCommand(res),
        m_id(id),
        m_value(value)
    {
    }

    virtual void beforeDisconnectFromResource() {}

    bool execute()
    {
        bool success = false;
        if (isConnectedToTheResource())
        {
            success = m_resource->setParamPhysical(m_id, m_value);
        }
        emit m_resource->asyncParamSetDone(m_resource, m_id, m_value, success);
        return success;
    }

private:
    QString m_id;
    QString m_value;
};
typedef std::shared_ptr<QnResourceSetParamCommand> QnResourceSetParamCommandPtr;

// -------------------------------------------------------------------------- //
// QnResourceSetParamsCommand
// -------------------------------------------------------------------------- //
class QnResourceSetParamsCommand: public QnResourceCommand
{
public:
    QnResourceSetParamsCommand(const QnResourcePtr &res, const QnCameraAdvancedParamValueList &values):
        QnResourceCommand(res),
        m_values(values)
    {
    }

    virtual void beforeDisconnectFromResource() {}

    bool execute()
    {
        bool success = isConnectedToTheResource();

        QnCameraAdvancedParamValueList result;
        if (success)
            success = m_resource->setParamsPhysical(m_values, result);
        emit m_resource->asyncParamsSetDone(m_resource, result);
        return success;
    }

private:
    QnCameraAdvancedParamValueList m_values;
};
typedef std::shared_ptr<QnResourceSetParamsCommand> QnResourceSetParamsCommandPtr;


#endif // ENABLE_DATA_PROVIDERS

// -------------------------------------------------------------------------- //
// QnResource
// -------------------------------------------------------------------------- //
QnResource::QnResource(QnCommonModule* commonModule):
    QObject(),
    m_mutex(QnMutex::Recursive),
    m_initMutex(QnMutex::Recursive),
    m_resourcePool(NULL),
    m_flags(0),
    m_initialized(false),
    m_lastInitTime(0),
    m_prevInitializationResult(CameraDiagnostics::ErrorCode::unknown),
    m_lastMediaIssue(CameraDiagnostics::NoErrorResult()),
    m_initInProgress(false),
    m_commonModule(commonModule)
{
}

QnResource::QnResource(const QnResource& right)
    :
    QObject(),
    m_parentId(right.m_parentId),
    m_name(right.m_name),
    m_url(right.m_url),
    m_resourcePool(right.m_resourcePool),
    m_id(right.m_id),
    m_typeId(right.m_typeId),
    m_flags(right.m_flags),
    m_lastDiscoveredTime(right.m_lastDiscoveredTime),
    m_tags(right.m_tags),
    m_initialized(right.m_initialized),
    m_lastInitTime(right.m_lastInitTime),
    m_prevInitializationResult(right.m_prevInitializationResult),
    m_lastMediaIssue(right.m_lastMediaIssue),
    m_initializationAttemptCount(right.m_initializationAttemptCount),
    m_locallySavedProperties(right.m_locallySavedProperties),
    m_initInProgress(right.m_initInProgress),
    m_commonModule(right.m_commonModule)
{
}

QnResource::~QnResource()
{
    disconnectAllConsumers();
}

QnResourcePool *QnResource::resourcePool() const
{
    QnMutexLocker mutexLocker(&m_mutex);

    return m_resourcePool;
}

void QnResource::setResourcePool(QnResourcePool *resourcePool)
{
    QnMutexLocker mutexLocker(&m_mutex);
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

bool QnResource::useLocalProperties() const
{
    return m_id.isNull();
}

void QnResource::updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers)
{
    // unique id MUST be the same
    NX_ASSERT(getId() == other->getId() || getUniqueId() == other->getUniqueId());
    NX_ASSERT(toSharedPointer(this));

    m_typeId = other->m_typeId;
    m_lastDiscoveredTime = other->m_lastDiscoveredTime;

    m_tags = other->m_tags;

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
        m_parentId = other->m_parentId;
        notifiers << [r = toSharedPointer(this)]{ emit r->parentIdChanged(r);};
        if (m_initialized)
        {
            m_initialized = false;
            notifiers << [r = toSharedPointer(this)]{ emit r->initializedChanged(r); };
        }
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
    {
        QnMutexLocker locker(&m_consumersMtx);
        for (QnResourceConsumer *consumer : m_consumers)
            consumer->beforeUpdate();
    }

    Qn::NotifierList notifiers;
    {
        QnMutex *m1 = &m_mutex, *m2 = &other->m_mutex;
        if (m1 > m2)
            std::swap(m1, m2);  //to maintain mutex lock order
        QnMutexLocker mutexLocker1(m1);
        QnMutexLocker mutexLocker2(m2);
        updateInternal(other, notifiers);
    }

    for (auto notifier : notifiers)
        notifier();

    {
        QnMutexLocker lk(&m_mutex);
        if (!useLocalProperties() && !m_locallySavedProperties.empty() && commonModule())
        {
            std::map<QString, LocalPropertyValue> locallySavedProperties;
            std::swap(locallySavedProperties, m_locallySavedProperties);
            QnUuid id = m_id;
            lk.unlock();

            for (auto prop : locallySavedProperties)
            {
                if (commonModule()->propertyDictionary()->setValue(
                    id,
                    prop.first,
                    prop.second.value,
                    prop.second.markDirty,
                    prop.second.replaceIfExists))   //isModified?
                {
                    emitPropertyChanged(prop.first);
                }
            }
        }
    }

    //silently ignoring missing properties because of removeProperty method lack
    for (const ec2::ApiResourceParamData &param : other->getRuntimeProperties())
        emitPropertyChanged(param.name);   //here "propertyChanged" will be called

    QnMutexLocker locker(&m_consumersMtx);
    for (QnResourceConsumer *consumer : m_consumers)
        consumer->afterUpdate();
}

QnUuid QnResource::getParentId() const
{
    QnMutexLocker locker(&m_mutex);
    return m_parentId;
}

void QnResource::setParentId(const QnUuid& parent)
{
    bool initializedChanged = false;
    QnUuid oldParentId;
    {
        QnMutexLocker locker(&m_mutex);
        if (m_parentId == parent)
            return;
        oldParentId = m_parentId;
        m_parentId = parent;
        if (m_initialized)
        {
            m_initialized = false;
            initializedChanged = true;
        }
    }

    emit parentIdChanged(toSharedPointer(this));

    if (initializedChanged)
        emit this->initializedChanged(toSharedPointer(this));
}


QString QnResource::getName() const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_name;
}

void QnResource::setName(const QString& name)
{
    {
        QnMutexLocker mutexLocker(&m_mutex);

        if (m_name == name)
            return;

        m_name = name;
    }

    emit nameChanged(toSharedPointer(this));
}

Qn::ResourceFlags QnResource::flags() const
{
    //QnMutexLocker mutexLocker( &m_mutex );
    return m_flags;
}

void QnResource::setFlags(Qn::ResourceFlags flags)
{
    {
        QnMutexLocker mutexLocker(&m_mutex);

        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

void QnResource::addFlags(Qn::ResourceFlags flags)
{
    {
        QnMutexLocker mutexLocker(&m_mutex);
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
        QnMutexLocker mutexLocker(&m_mutex);
        flags = m_flags & ~flags;
        if (m_flags == flags)
            return;

        m_flags = flags;
    }
    emit flagsChanged(toSharedPointer(this));
}

QString QnResource::toSearchString() const
{
    return getId().toSimpleString() + L' ' + getName();
}

QnResourcePtr QnResource::getParentResource() const
{
    QnUuid parentID;
    QnResourcePool* resourcePool = NULL;
    {
        QnMutexLocker mutexLocker(&m_mutex);
        parentID = getParentId();
        resourcePool = m_resourcePool;
    }

    if (resourcePool)
        return resourcePool->getResourceById(parentID);
    else
        return QnResourcePtr();
}

bool QnResource::hasParam(const QString &name) const
{
    QnResourceTypePtr resType = qnResTypePool->getResourceType(m_typeId);
    if (!resType)
        return false;
    return resType->hasParam(name);
}

bool QnResource::getParamPhysical(const QString &id, QString &value)
{
    Q_UNUSED(id)
    Q_UNUSED(value)
    return false;
}

bool QnResource::getParamsPhysical(const QSet<QString> &idList, QnCameraAdvancedParamValueList& result)
{
    bool success = true;
    for (const QString &id : idList)
    {
        QString value;
        if (getParamPhysical(id, value))
            result << QnCameraAdvancedParamValue(id, value);
        else
            success = false;
    }
    return success;
}

bool QnResource::setParamPhysical(const QString &id, const QString &value)
{
    Q_UNUSED(id)
    Q_UNUSED(value)
    return false;
}

bool QnResource::setParamsPhysical(const QnCameraAdvancedParamValueList &valueList, QnCameraAdvancedParamValueList &result)
{
    bool success = true;
    setParamsBegin();
    for (const QnCameraAdvancedParamValue &value : valueList)
    {
        if (setParamPhysical(value.id, value.value))
            result << value;
        else
            success = false;
    }
    setParamsEnd();
    return success;
}

bool QnResource::setParamsBegin()
{
    return false;
}

bool QnResource::setParamsEnd()
{
    return false;
}


#ifdef ENABLE_DATA_PROVIDERS

void QnResource::setParamPhysicalAsync(const QString& name, const QString& value)
{
    QnResourceSetParamCommandPtr command(new QnResourceSetParamCommand(toSharedPointer(this), name, value));
    addCommandToProc(command);
}

void QnResource::getParamPhysicalAsync(const QString& name)
{
    QnResourceGetParamCommandPtr command(new QnResourceGetParamCommand(toSharedPointer(this), name));
    addCommandToProc(command);
}

void QnResource::getParamsPhysicalAsync(const QSet<QString> &ids)
{
    QnResourceGetParamsCommandPtr command(new QnResourceGetParamsCommand(toSharedPointer(this), ids));
    addCommandToProc(command);
}

void QnResource::setParamsPhysicalAsync(const QnCameraAdvancedParamValueList &values)
{
    QnResourceSetParamsCommandPtr command(new QnResourceSetParamsCommand(toSharedPointer(this), values));
    addCommandToProc(command);
}

#endif

bool QnResource::unknownResource() const
{
    return getName().isEmpty();
}

QnUuid QnResource::getTypeId() const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_typeId;
}

void QnResource::setTypeId(const QnUuid &id)
{
    if (id.isNull())
    {
        qWarning() << "NULL typeId is set to resource" << getName();
        return;
    }

    QnMutexLocker mutexLocker(&m_mutex);
    m_typeId = id;
}

void QnResource::setTypeByName(const QString& resTypeName)
{
    QnResourceTypePtr resType = qnResTypePool->getResourceTypeByName(resTypeName);
    if (resType)
        setTypeId(resType->getId());
}

Qn::ResourceStatus QnResource::getStatus() const
{
    NX_EXPECT(commonModule());
    return commonModule()
        ? commonModule()->statusDictionary()->value(getId())
        : Qn::NotDefined;
}

void QnResource::doStatusChanged(Qn::ResourceStatus oldStatus, Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
#ifdef QN_RESOURCE_DEBUG
    qDebug() << "Change status. oldValue=" << oldStatus << " new value=" << newStatus << " id=" << m_id << " name=" << getName();
#endif

    if (newStatus == Qn::Offline || newStatus == Qn::Unauthorized)
    {
        if (m_initialized)
        {
            m_initialized = false;
            emit initializedChanged(toSharedPointer(this));
        }
    }

    // Null pointer if we are changing status in constructor. Signal is not needed in this case.
    if (auto sharedThis = toSharedPointer(this))
    {
        NX_LOG(lit("%1 Emit statusChanged signal for resource %2, %3, %4")
                .arg(QString::fromLatin1(Q_FUNC_INFO))
                .arg(sharedThis->getId().toString())
                .arg(sharedThis->getName())
                .arg(sharedThis->getUrl()), cl_logDEBUG2);

        emit statusChanged(sharedThis, reason);
    }
}

void QnResource::setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason)
{
    if (newStatus == Qn::NotDefined)
        return;

    if (hasFlags(Qn::removed))
        return;

    NX_EXPECT(commonModule());
    if (!commonModule())
        return;

    QnUuid id = getId();
    Qn::ResourceStatus oldStatus = commonModule()->statusDictionary()->value(id);
    if (oldStatus == newStatus)
        return;
    commonModule()->statusDictionary()->setValue(id, newStatus);
    doStatusChanged(oldStatus, newStatus, reason);
}

QDateTime QnResource::getLastDiscoveredTime() const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_lastDiscoveredTime;
}

void QnResource::setLastDiscoveredTime(const QDateTime &time)
{
    QnMutexLocker mutexLocker(&m_mutex);
    m_lastDiscoveredTime = time;
}

QnUuid QnResource::getId() const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_id;
}

void QnResource::setId(const QnUuid& id)
{
    QnMutexLocker mutexLocker(&m_mutex);

    // TODO: #dmishin it seems really wrong. Think about how to do it in another way.
    NX_ASSERT(
        dynamic_cast<QnSecurityCamResource*>(this) || m_locallySavedProperties.size() == 0,
        lit("Only camera resources are allowed to set properties if id is not set."));

    m_id = id;
}

QString QnResource::getUrl() const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_url;
}

void QnResource::setUrl(const QString &url)
{
    {
        QnMutexLocker mutexLocker(&m_mutex);
        if (m_url == url)
            return;
        m_url = url;
    }
    emit urlChanged(toSharedPointer(this));
}

void QnResource::addTag(const QString& tag)
{
    QnMutexLocker mutexLocker(&m_mutex);
    if (!m_tags.contains(tag))
        m_tags.push_back(tag);
}

void QnResource::setTags(const QStringList& tags)
{
    QnMutexLocker mutexLocker(&m_mutex);
    m_tags = tags;
}

void QnResource::removeTag(const QString& tag)
{
    QnMutexLocker mutexLocker(&m_mutex);
    m_tags.removeAll(tag);
}

bool QnResource::hasTag(const QString& tag) const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_tags.contains(tag);
}

QStringList QnResource::getTags() const
{
    QnMutexLocker mutexLocker(&m_mutex);
    return m_tags;
}

void QnResource::addConsumer(QnResourceConsumer *consumer)
{
    QnMutexLocker locker(&m_consumersMtx);

    if (m_consumers.contains(consumer))
    {
        qnWarning("Given resource consumer '%1' is already associated with this resource.", typeid(*consumer).name());
        return;
    }

    m_consumers.insert(consumer);
}

void QnResource::removeConsumer(QnResourceConsumer *consumer)
{
    QnMutexLocker locker(&m_consumersMtx);

    m_consumers.remove(consumer);
}

bool QnResource::hasConsumer(QnResourceConsumer *consumer) const
{
    QnMutexLocker locker(&m_consumersMtx);
    return m_consumers.contains(consumer);
}

#ifdef ENABLE_DATA_PROVIDERS
bool QnResource::hasUnprocessedCommands() const
{
    QnMutexLocker locker(&m_consumersMtx);
    for (QnResourceConsumer* consumer : m_consumers)
    {
        if (dynamic_cast<QnResourceCommand*>(consumer))
            return true;
    }

    return false;
}
#endif

void QnResource::disconnectAllConsumers()
{
    QnMutexLocker locker(&m_consumersMtx);

    for (QnResourceConsumer *consumer : m_consumers)
        consumer->beforeDisconnectFromResource();

    for (QnResourceConsumer *consumer : m_consumers)
        consumer->disconnectFromResource();

    m_consumers.clear();
}

#ifdef ENABLE_DATA_PROVIDERS
QnAbstractStreamDataProvider* QnResource::createDataProvider(Qn::ConnectionRole role)
{
    QnAbstractStreamDataProvider* dataProvider = createDataProviderInternal(role);

    if (dataProvider != NULL && dataProvider->getResource() != this)
        qnCritical("createDataProviderInternal() returned a data provider that is not associated with current resource."); /* This may cause hard to debug problems. */

    return dataProvider;
}

QnAbstractStreamDataProvider *QnResource::createDataProviderInternal(Qn::ConnectionRole)
{
    return NULL;
}
#endif

QnAbstractPtzController *QnResource::createPtzController()
{
    QnAbstractPtzController *result = createPtzControllerInternal();
    if (!result)
        return result;

    /* Do some sanity checking. */
    Ptz::Capabilities capabilities = result->getCapabilities();
    if((capabilities & Ptz::LogicalPositioningPtzCapability) && !(capabilities & Ptz::AbsolutePtzCapabilities))
    {
        auto message =
            lit("Logical position space capability is defined for a PTZ controller that does not support absolute movement. %1 %2")
                .arg(getName())
                .arg(getUrl());

        qDebug() << message;
        NX_LOG(message, cl_logWARNING);
    }

    if((capabilities & Ptz::DevicePositioningPtzCapability) && !(capabilities & Ptz::AbsolutePtzCapabilities))
    {
        auto message =
            lit("Device position space capability is defined for a PTZ controller that does not support absolute movement. %1 %2")
                .arg(getName())
                .arg(getUrl());

        qDebug() << message;
        NX_LOG(message.toLatin1(), cl_logERROR);
    }

    return result;
}

QnAbstractPtzController *QnResource::createPtzControllerInternal()
{
    return NULL;
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

    return commonModule()->propertyDictionary()->hasProperty(getId(), key);
}

QString QnResource::getProperty(const QString &key) const
{
    QString value;
    {
        QnMutexLocker lk(&m_mutex);
        if (useLocalProperties())
        {
            auto itr = m_locallySavedProperties.find(key);
            if (itr != m_locallySavedProperties.end())
                value = itr->second.value;
        }
        else if (auto module = commonModule())
        {
            value = module->propertyDictionary()->value(m_id, key);
        }
    }

    if (value.isNull())
    {
        // find default value in resourceType
        QnMutexLocker lk(&m_mutex);
        QnResourceTypePtr resType = qnResTypePool->getResourceType(m_typeId);
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
    // TODO: #GDM think about code duplication
    NX_ASSERT(!resourceId.isNull() && !resourceTypeId.isNull(), Q_FUNC_INFO, "Invalid input, reading from local data is requred.");

    NX_EXPECT(commonModule);
    QString value = commonModule
        ? commonModule->propertyDictionary()->value(resourceId, key)
        : QString();

    if (value.isNull())
    {
        // find default value in resourceType
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
        QnMutexLocker lk(&m_mutex);
        if (useLocalProperties())
        {
            //saving property to some internal dictionary. Will apply to global dictionary when id is known
            m_locallySavedProperties[key] = LocalPropertyValue(value, markDirty, replaceIfExists);

            //calling propertyDictionary()->saveParams(...) does not make any sense
            return false;
        }
    }

    NX_ASSERT(!getId().isNull());
    NX_EXPECT(commonModule());

    bool isModified = commonModule() && commonModule()->propertyDictionary()->setValue(
        getId(), key, value, markDirty, replaceIfExists);

    if (isModified)
        emitPropertyChanged(key);

    return isModified;
}

bool QnResource::removeProperty(const QString& key)
{
    {
        QnMutexLocker lk(&m_mutex);
        if (useLocalProperties())
        {
            m_locallySavedProperties.erase(key);
            return false;
        }
    }

    NX_ASSERT(!getId().isNull());
    NX_EXPECT(commonModule());
    if (!commonModule())
        return false;

    commonModule()->propertyDictionary()->removeProperty(getId(), key);
    emitPropertyChanged(key);

    return true;
}

bool QnResource::setProperty(const QString &key, const QVariant& value, PropertyOptions options)
{
    return setProperty(key, value.toString(), options);
}

void QnResource::emitPropertyChanged(const QString& key)
{
    if (key == Qn::PTZ_CAPABILITIES_PARAM_NAME)
        emit ptzCapabilitiesChanged(::toSharedPointer(this));
    else if (key == Qn::VIDEO_LAYOUT_PARAM_NAME)
        emit videoLayoutChanged(::toSharedPointer(this));

    emit propertyChanged(toSharedPointer(this), key);
}

ec2::ApiResourceParamDataList QnResource::getRuntimeProperties() const
{
    if (useLocalProperties())
    {
        ec2::ApiResourceParamDataList result;
        for (auto itr = m_locallySavedProperties.begin(); itr != m_locallySavedProperties.end(); ++itr)
            result.push_back(ec2::ApiResourceParamData(itr->first, itr->second.value));
        return result;
    }
    else if (auto module = commonModule())
    {
        return module->propertyDictionary()->allProperties(getId());
    }

    return ec2::ApiResourceParamDataList();
}

ec2::ApiResourceParamDataList QnResource::getAllProperties() const
{
    ec2::ApiResourceParamDataList result;
    ec2::ApiResourceParamDataList runtimeProperties;
    if (auto module = commonModule())
        runtimeProperties = module->propertyDictionary()->allProperties(getId());

    ParamTypeMap staticDefaultProperties;

    QnResourceTypePtr resType = qnResTypePool->getResourceType(getTypeId());
    if (resType)
        staticDefaultProperties = resType->paramTypeList();

    for (auto it = staticDefaultProperties.cbegin(); it != staticDefaultProperties.cend(); ++it)
    {
        auto runtimeIt = std::find_if(runtimeProperties.cbegin(), runtimeProperties.cend(),
            [it](const ec2::ApiResourceParamData &param) { return it.key() == param.name; });
        if (runtimeIt == runtimeProperties.cend())
            result.emplace_back(it.key(), it.value());
    }

#if 0
    printf("\n==== Camera: %s ====\n", getName().toLatin1().constData());
    printf("\tStatic properties:\n");
    for (auto it = staticDefaultProperties.cbegin(); it != staticDefaultProperties.cend(); ++it)
        printf("\t\t%s = %s\n", it.key().toLatin1().constData(), it.value().toLatin1().constData());

    printf("\tRuntime properties:\n");
    for (auto it = runtimeProperties.cbegin(); it != runtimeProperties.cend(); ++it)
        printf("\t\t%s = %s\n", it->name.toLatin1().constData(), it->value.toLatin1().constData());

    printf("\n");
#endif

    std::copy(runtimeProperties.cbegin(), runtimeProperties.cend(), std::back_inserter(result));
    return result;
}

void QnResource::emitModificationSignals(const QSet<QByteArray>& modifiedFields)
{
    emit resourceChanged(toSharedPointer(this));
    //modifiedFields << "resourceChanged";

    const QnResourcePtr & _t1 = toSharedPointer(this);
    void *_a[] = {0, const_cast<void*>(reinterpret_cast<const void*>(&_t1))};
    for (const QByteArray& signalName : modifiedFields)
        emitDynamicSignal((signalName + QByteArray("(QnResourcePtr)")).data(), _a);
}

QnInitResPool* QnResource::initAsyncPoolInstance()
{
    return initResPool();
}
// -----------------------------------------------------------------------------

#ifdef ENABLE_DATA_PROVIDERS
Q_GLOBAL_STATIC(QnResourceCommandProcessor, QnResourceCommandProcessor_instance)
static bool qnResourceCommandProcessorInitialized = false;

void QnResource::startCommandProc()
{
    QnResourceCommandProcessor_instance()->start();
    qnResourceCommandProcessorInitialized = true;
}

void QnResource::stopCommandProc()
{
    if (qnResourceCommandProcessorInitialized)
        QnResourceCommandProcessor_instance()->stop();
}

void QnResource::addCommandToProc(const QnResourceCommandPtr& command)
{
    NX_ASSERT(qnResourceCommandProcessorInitialized, Q_FUNC_INFO, "Processor is not started");
    if (qnResourceCommandProcessorInitialized)
        QnResourceCommandProcessor_instance()->putData(command);
}

int QnResource::commandProcQueueSize()
{
    NX_ASSERT(qnResourceCommandProcessorInitialized, Q_FUNC_INFO, "Processor is not started");
    if (qnResourceCommandProcessorInitialized)
        return QnResourceCommandProcessor_instance()->queueSize();
    return 0;
}
#endif // ENABLE_DATA_PROVIDERS

bool QnResource::init()
{
    if (m_appStopping)
        return false;

    {
        QnMutexLocker lock(&m_initMutex);
        if (m_initialized)
            return true; /* Nothing to do. */
        if (m_initInProgress)
            return false; /* Skip request if init is already running. */
        m_initInProgress = true;
    }

    CameraDiagnostics::Result initResult = initInternal();
    m_initMutex.lock();
    m_initInProgress = false;
    m_initialized = initResult.errorCode == CameraDiagnostics::ErrorCode::noError;
    {
        QnMutexLocker lk(&m_mutex);
        m_prevInitializationResult = initResult;
    }

    m_initializationAttemptCount.fetchAndAddOrdered(1);

    bool changed = m_initialized;
    if (m_initialized)
        initializationDone();
    else if (getStatus() == Qn::Online || getStatus() == Qn::Recording)
        setStatus(Qn::Offline);

    m_initMutex.unlock();

    if (changed)
        emit initializedChanged(toSharedPointer(this));

    return true;
}

void QnResource::setLastMediaIssue(const CameraDiagnostics::Result& issue)
{
    QnMutexLocker lk(&m_mutex);
    m_lastMediaIssue = issue;
}

CameraDiagnostics::Result QnResource::getLastMediaIssue() const
{
    QnMutexLocker lk(&m_mutex);
    return m_lastMediaIssue;
}

void QnResource::blockingInit()
{
    if (!init())
    {
        //init is running in another thread, waiting for it to complete...
        QnMutexLocker lk(&m_initMutex);
    }
}

void QnResource::initAndEmit()
{
    init();
}

class InitAsyncTask: public QRunnable
{
public:
    InitAsyncTask(QnResourcePtr resource): m_resource(resource) {}
    void run()
    {
        m_resource->initAndEmit();
    }
private:
    QnResourcePtr m_resource;
};


void QnResource::stopAsyncTasks()
{
    pleaseStopAsyncTasks();
    initResPool()->waitForDone();
}

void QnResource::pleaseStopAsyncTasks()
{
    QnMutexLocker lock(&m_initAsyncMutex);
    m_appStopping = true;
}

void QnResource::initAsync(bool optional)
{
    qint64 t = getUsecTimer();

    QnMutexLocker lock(&m_initAsyncMutex);

    if (t - m_lastInitTime < MIN_INIT_INTERVAL)
        return;

    if (m_appStopping)
        return;

    if (hasFlags(Qn::foreigner))
        return; // removed to other server

    InitAsyncTask *task = new InitAsyncTask(toSharedPointer(this));
    if (optional)
    {
        if (initResPool()->tryStart(task))
            m_lastInitTime = t;
        else
            delete task;
    }
    else
    {
        m_lastInitTime = t;
        initResPool()->start(task);
    }
}

CameraDiagnostics::Result QnResource::prevInitializationResult() const
{
    QnMutexLocker lk(&m_mutex);
    return m_prevInitializationResult;
}

int QnResource::initializationAttemptCount() const
{
    return m_initializationAttemptCount.load();
}

bool QnResource::isInitialized() const
{
    return m_initialized;
}

void QnResource::setUniqId(const QString& value)
{
    Q_UNUSED(value)
    NX_ASSERT(false, Q_FUNC_INFO, "Not implemented");
}

Ptz::Capabilities QnResource::getPtzCapabilities() const
{
    return Ptz::Capabilities(getProperty(Qn::PTZ_CAPABILITIES_PARAM_NAME).toInt());
}

bool QnResource::hasAnyOfPtzCapabilities(Ptz::Capabilities capabilities) const
{
    return getPtzCapabilities() & capabilities;
}

void QnResource::setPtzCapabilities(Ptz::Capabilities capabilities)
{
    if (hasParam(Qn::PTZ_CAPABILITIES_PARAM_NAME))
        setProperty(Qn::PTZ_CAPABILITIES_PARAM_NAME, static_cast<int>(capabilities));
}

void QnResource::setPtzCapability(Ptz::Capabilities capability, bool value)
{
    setPtzCapabilities(value ? (getPtzCapabilities() | capability) : (getPtzCapabilities() & ~capability));
}

void QnResource::setCommonModule(QnCommonModule* commonModule)
{
    QnMutexLocker mutexLocker(&m_mutex);
    m_commonModule = commonModule;
}

QnCommonModule* QnResource::commonModule() const
{
    {
        QnMutexLocker mutexLocker(&m_mutex);
        if (m_commonModule)
            return m_commonModule;
    }

    if (const auto pool = resourcePool())
        return pool->commonModule();

    return nullptr;
}

QString QnResource::idForToStringFromPtr() const
{
    return getId().toSimpleString();
}

bool QnResource::saveParams()
{
    NX_EXPECT(commonModule() && !getId().isNull());
    if (auto module = commonModule())
        return module->propertyDictionary()->saveParams(getId());
    return false;
}

int QnResource::saveParamsAsync()
{
    NX_EXPECT(commonModule() && !getId().isNull());
    if (auto module = commonModule())
        return module->propertyDictionary()->saveParamsAsync(getId());
    return false;
}

