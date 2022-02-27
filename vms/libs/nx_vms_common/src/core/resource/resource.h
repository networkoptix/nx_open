// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>

#include <QtCore/QMetaType>
#include <QtCore/QSet>

#include <utils/camera/camera_diagnostics.h>
#include <utils/common/from_this_to_shared.h>
#include <utils/common/functional.h>
#include <common/common_globals.h>

#include <nx/utils/elapsed_timer.h>

#include "shared_resource_pointer.h"
#include "resource_fwd.h"
#include "resource_type.h"

class QnResourceConsumer;
class QnResourcePool;
class QnCommonModule;

namespace nx::vms::api {

struct ResourceParamData;
using ResourceParamDataList = std::vector<ResourceParamData>;

} // namespace nx::vms::api

class NX_VMS_COMMON_API QnResource:
    public QObject,
    public QnFromThisToShared<QnResource>
{
    Q_OBJECT
    Q_FLAGS(Qn::ResourceFlags)
    Q_PROPERTY(QnUuid id READ getId CONSTANT)
    Q_PROPERTY(QnUuid typeId READ getTypeId CONSTANT)
    Q_PROPERTY(QString uniqueId READ getUniqueId CONSTANT)
    Q_PROPERTY(int logicalId READ logicalId WRITE setLogicalId NOTIFY logicalIdChanged)
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QnUuid parentId READ getParentId WRITE setParentId NOTIFY parentIdChanged)
    Q_PROPERTY(Qn::ResourceFlags flags READ flags WRITE setFlags NOTIFY flagsChanged)
    Q_PROPERTY(QString url READ getUrl WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(nx::vms::api::ResourceStatus status READ getStatus WRITE setStatus NOTIFY statusChanged)
public:

    QnResource(QnCommonModule* commonModule = nullptr);
    QnResource(const QnResource&);
    virtual ~QnResource();

    virtual const QnUuid& getId() const { return m_id;  }

    QnUuid getParentId() const;
    virtual void setParentId(const QnUuid& parent);

    // device unique identifier
    virtual QString getUniqueId() const { return getId().toString(); }
    virtual void setUniqId(const QString& value);

    // TypeId unique string id for resource with SUCH list of params and CLASS
    // in other words TypeId can be used instantiate the right resource
    QnUuid getTypeId() const;
    void setTypeId(const QnUuid &id);
    void setTypeByName(const QString& resTypeName);

    virtual nx::vms::api::ResourceStatus getStatus() const;
    nx::vms::api::ResourceStatus getPreviousStatus() const;
    virtual void setStatus(nx::vms::api::ResourceStatus newStatus, Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local);

    virtual bool isOnline() const { return isOnline(getStatus()); }
    static bool isOnline(nx::vms::api::ResourceStatus status)
    {
        return status == nx::vms::api::ResourceStatus::online || status == nx::vms::api::ResourceStatus::recording;
    }

    //!this function is called if resource changes state from offline to online or so
    /*!
        \note If \a QnResource::init is already running in another thread, this method exits immediately and returns false
        \return true, if initialization attempt is done (with success or failure). false, if \a QnResource::init is already running in other thread
    */
    virtual bool init();

    /**
     * Initialize camera asynchronously. If a camera has been already initialized or it is
     * initializing now this call does nothing.
     */
    void initAsync();

    /**
     * Initialize camera asynchronously if reinit timeout has expired or if the camera properties
     * have changed.
     */
    void tryToInitAsync();

    /**
     * Reinitialize camera asynchronously. If a camera has been already initialized or it is
     * initializing now the new initialization will be scheduled anyway.
     */
    void reinitAsync();

    CameraDiagnostics::Result prevInitializationResult() const;

    // flags like network media and so on
    virtual Qn::ResourceFlags flags() const;
    bool hasFlags(Qn::ResourceFlags flags) const { return (this->flags() & flags) == flags; }
    void setFlags(Qn::ResourceFlags flags);
    void addFlags(Qn::ResourceFlags flags);
    void removeFlags(Qn::ResourceFlags flags);

    //just a simple resource name
    virtual QString getName() const;
    virtual void setName(const QString& name);

    QnResourcePool *resourcePool() const;
    virtual void setResourcePool(QnResourcePool *resourcePool);

    template<class Resource>
    static QnSharedResourcePointer<Resource> toSharedPointer(const Resource *resource);

    QnResourcePtr getParentResource() const;

    // ==================================================

    virtual QString getUrl() const;
    virtual void setUrl(const QString &url);

    virtual int logicalId() const;
    virtual void setLogicalId(int value);

    bool hasConsumer(QnResourceConsumer *consumer) const;

    virtual bool isInitialized() const;
    virtual bool isInitializationInProgress() const;

    bool hasDefaultProperty(const QString &name) const;

    /* Note that these functions hide property API inherited from QObject.
     * This is intended as this API cannot be used with QnResource anyway
     * because of threading issues. */

    virtual bool hasProperty(const QString &key) const;
    virtual QString getProperty(const QString &key) const;
    static QString getResourceProperty(
        QnCommonModule* commonModule,
        const QString& key,
        const QnUuid &resourceId,
        const QnUuid &resourceTypeId);

    nx::vms::api::ResourceParamDataList getRuntimeProperties() const;
    nx::vms::api::ResourceParamDataList getAllProperties() const;

    enum PropertyOptions
    {
        DEFAULT_OPTIONS     = 0,
        NO_MARK_DIRTY       = 1 << 0,
        NO_REPLACE_IF_EXIST = 1 << 1,
        NO_ALLOW_EMPTY      = 1 << 2,
    };

    /** @return Whether the stored property value has been modified by this call. */
    virtual bool setProperty(
        const QString &key,
        const QString &value,
        PropertyOptions options = DEFAULT_OPTIONS);

    virtual bool setProperty(
        const QString &key,
        const QVariant& value,
        PropertyOptions options = DEFAULT_OPTIONS);

    template<typename Update>
    bool updateProperty(const QString &key, const Update& update)
    {
        NX_MUTEX_LOCKER lk(&m_mutex); // recursive
        return setProperty(key, update(getProperty(key)));
    }

    //!Call this with proper field names to emit corresponding *changed signals.
    // Signal can be defined in a derived class.
    void emitModificationSignals(const QSet<QByteArray>& modifiedFields);

    virtual bool saveProperties();
    virtual int savePropertiesAsync();
    void setForceUseLocalProperties(bool value);
signals:
    void parameterValueChanged(const QnResourcePtr& resource, const QString& param) const;
    void statusChanged(const QnResourcePtr& resource, Qn::StatusChangeReason reason);
    void nameChanged(const QnResourcePtr& resource);
    void parentIdChanged(const QnResourcePtr& resource, const QnUuid& previousParentId);
    void flagsChanged(const QnResourcePtr& resource);
    void urlChanged(const QnResourcePtr& resource);
    void logicalIdChanged(const QnResourcePtr& resource);
    void resourceChanged(const QnResourcePtr& resource);
    void propertyChanged(const QnResourcePtr& resource, const QString& key, const QString& prevValue, const QString& newValue);
    void initializedChanged(const QnResourcePtr& resource);

    /**
     *  These signals should be removed as soon as QnMediaRessource will be fixed.
     */
    void mediaDewarpingParamsChanged(const QnResourcePtr& resource);
    void videoLayoutChanged(const QnResourcePtr& resource);
    void rotationChanged();
public:
    void update(const QnResourcePtr& other);

    // Need use lock/unlock consumers before this call!
    QSet<QnResourceConsumer *> getAllConsumers() const { return m_consumers; }
    void lockConsumers() { m_consumersMtx.lock(); }
    void unlockConsumers() { m_consumersMtx.unlock(); }

    QnResourcePtr toSharedPointer() const;

    virtual void setCommonModule(QnCommonModule* commonModule);
    QnCommonModule* commonModule() const;

    virtual QString idForToStringFromPtr() const; //< Used by toString(const T*).

    void setIdUnsafe(const QnUuid& id);
protected:
    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers);

    virtual CameraDiagnostics::Result initInternal();
    //!Called just after successful \a initInternal()
    /*!
        Inherited class implementation MUST call base class method first
    */
    virtual void initializationDone();

    virtual void emitPropertyChanged(
        const QString& key, const QString& prevValue, const QString& newValue);

    /**
     * Update url value without mutex locking.
     * @return if value was actually changed.
     */
    bool setUrlUnsafe(const QString& value);
private:

    /* The following consumer-related API is private as it is supposed to be used from QnResourceConsumer instances only.
     * Using it from other places may break invariants. */
    friend class QnResourceConsumer;

    void addConsumer(QnResourceConsumer *consumer);
    void removeConsumer(QnResourceConsumer *consumer);
    void disconnectAllConsumers();

    bool emitDynamicSignal(const char *signal, void **arguments);

    bool useLocalProperties() const;

    friend class InitAsyncTask;

    enum InitState
    {
        initNone,
        initInProgress,
        reinitRequested,
        initDone
    };
    bool switchState(InitState from, InitState to);
protected:
    /** Mutex that is to be used when accessing a set of all consumers. */
    mutable nx::Mutex m_consumersMtx;

    /** Set of consumers for this resource. */
    QSet<QnResourceConsumer *> m_consumers;

    /** Mutex that is to be used when accessing resource fields. */
    mutable nx::Mutex m_mutex;

    /** Identifier of the parent resource. Use resource pool to retrieve the actual parent resource. */
    QnUuid m_parentId;

    /** Name of this resource. */
    QString m_name;

    /** Url of this resource, if any. */
    QString m_url;
private:
    struct LocalPropertyValue
    {
        QString value;
        bool markDirty;
        bool replaceIfExists;

        LocalPropertyValue()
            :
            markDirty(false),
            replaceIfExists(false)
        {
        }

        LocalPropertyValue(
            const QString& _value,
            bool _markDirty,
            bool _replaceIfExists)
            :
            value(_value),
            markDirty(_markDirty),
            replaceIfExists(_replaceIfExists)
        {
        }
    };

    /** Resource pool this resource belongs to. */
    std::atomic<QnResourcePool*> m_resourcePool{};

    /** Identifier of this resource. */
    QnUuid m_id;

    /** Identifier of the type of this resource. */
    QnUuid m_typeId;

    /** Flags of this resource that determine its type. */
    Qn::ResourceFlags m_flags;

    CameraDiagnostics::Result m_prevInitializationResult = CameraDiagnostics::Result(
        CameraDiagnostics::ErrorCode::initializationInProgress);

    //!map<key, <value, isDirty>>
    std::map<QString, LocalPropertyValue> m_locallySavedProperties;

    std::atomic<InitState> m_initState{initNone};

    std::atomic<QnCommonModule*> m_commonModule{};
    std::atomic<bool> m_forceUseLocalProperties{};
    std::atomic<int> cTestStatus{0};
    std::atomic<nx::vms::api::ResourceStatus> m_previousStatus = nx::vms::api::ResourceStatus::undefined;
    mutable nx::Mutex m_initMutex;

    nx::utils::SafeElapsedTimer m_elapsedSinceLastInit;
    nx::utils::SafeElapsedTimer m_tryToInitTimer;
};

template<class Resource>
QnSharedResourcePointer<Resource> toSharedPointer(const Resource *resource)
{
    if (!resource)
        return QnSharedResourcePointer<Resource>();
    return resource->toSharedPointer().template staticCast<Resource>();
}

template<class Resource>
QnSharedResourcePointer<Resource> QnResource::toSharedPointer(const Resource *resource)
{
    using ::toSharedPointer; /* Let ADL kick in. */
    return toSharedPointer(resource);
}

Q_DECLARE_METATYPE(QnResourcePtr);
Q_DECLARE_METATYPE(QnResourceList);
