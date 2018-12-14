#pragma once

#include <atomic>

#include <QtCore/QMetaType>
#include <QtCore/QSet>
#include <QtCore/QThreadPool>

#include <api/model/kvpair.h>

#include <nx/utils/url.h>
#include <utils/camera/camera_diagnostics.h>
#include <utils/common/from_this_to_shared.h>

#include <utils/common/id.h>
#include <utils/common/functional.h>

#include <common/common_globals.h>
#include "shared_resource_pointer.h"
#include "resource_fwd.h"
#include "resource_type.h"

class QnResourceConsumer;
class QnResourcePool;
class QnCommonModule;

class QnResource: public QObject, public QnFromThisToShared<QnResource>
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
public:

    QnResource(QnCommonModule* commonModule = nullptr);
    QnResource(const QnResource&);
    virtual ~QnResource();

    QnUuid getId() const;
    void setId(const QnUuid& id);

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

    virtual Qn::ResourceStatus getStatus() const;
    virtual void setStatus(Qn::ResourceStatus newStatus, Qn::StatusChangeReason reason = Qn::StatusChangeReason::Local);

    //!this function is called if resource changes state from offline to online or so
    /*!
        \note If \a QnResource::init is already running in another thread, this method exits immediately and returns false
        \return true, if initialization attempt is done (with success or failure). false, if \a QnResource::init is already running in other thread
    */
    bool init();

    /**
     * Initialize camera sync. This function can omit initialization if recently call was failed.
     * It init camera not often then some time period.
     * @param optional - if false and no thread in ThreadPool left then return immediately.
     * if true and no thread in ThreadPool left then add new thread to ThreadPool queue.
     */
    void initAsync(bool optional);

    /**
     * Same as initAsync but run initialization always.
     * This call don't check if initAsync was called recently but always add a new task.
     */
    void reinitAsync();

    CameraDiagnostics::Result prevInitializationResult() const;
    //!Returns counter of resource initialization attempts (every attempt: successful or not)
    int initializationAttemptCount() const;

    // flags like network media and so on
    virtual Qn::ResourceFlags flags() const;
    inline bool hasFlags(Qn::ResourceFlags flags) const { return (this->flags() & flags) == flags; }
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

    nx::utils::Url url() const;
    void setUrl(const nx::utils::Url& url);

    virtual QString getUrl() const; //< Unsafe, deprecated. url() should be used instead.
    virtual void setUrl(const QString &url); //< Unsafe, deprecated. setUrl() should be used instead.

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
        QnMutexLocker lk(&m_mutex); // recursive
        return setProperty(key, update(getProperty(key)));
    }

    //!Call this with proper field names to emit corresponding *changed signals.
    // Signal can be defined in a derived class.
    void emitModificationSignals(const QSet<QByteArray>& modifiedFields);

    virtual bool saveProperties();
    virtual int savePropertiesAsync();
signals:
    void parameterValueChanged(const QnResourcePtr &resource, const QString &param) const;
    void statusChanged(const QnResourcePtr &resource, Qn::StatusChangeReason reason);
    void nameChanged(const QnResourcePtr &resource);
    void parentIdChanged(const QnResourcePtr &resource);
    void flagsChanged(const QnResourcePtr &resource);
    void urlChanged(const QnResourcePtr &resource);
    void logicalIdChanged(const QnResourcePtr& resource);
    void resourceChanged(const QnResourcePtr &resource);
    void mediaDewarpingParamsChanged(const QnResourcePtr &resource);
    void propertyChanged(const QnResourcePtr &resource, const QString &key);
    void initializedChanged(const QnResourcePtr &resource);
    void videoLayoutChanged(const QnResourcePtr &resource);

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

protected:
    virtual void updateInternal(const QnResourcePtr &other, Qn::NotifierList& notifiers);

    virtual CameraDiagnostics::Result initInternal();
    //!Called just after successful \a initInternal()
    /*!
        Inherited class implementation MUST call base class method first
    */
    virtual void initializationDone();

    virtual void emitPropertyChanged(const QString& key);
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

protected:
    /** Mutex that is to be used when accessing a set of all consumers. */
    mutable QnMutex m_consumersMtx;

    /** Set of consumers for this resource. */
    QSet<QnResourceConsumer *> m_consumers;

    /** Mutex that is to be used when accessing resource fields. */
    mutable QnMutex m_mutex;
    mutable QnMutex m_initMutex;

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
    QnResourcePool* m_resourcePool = nullptr;

    /** Identifier of this resource. */
    QnUuid m_id;

    /** Identifier of the type of this resource. */
    QnUuid m_typeId;

    /** Flags of this resource that determine its type. */
    Qn::ResourceFlags m_flags = 0;

    std::atomic<bool> m_initialized{false};
    static QnMutex m_initAsyncMutex;

    qint64 m_lastInitTime = 0;
    CameraDiagnostics::Result m_prevInitializationResult = CameraDiagnostics::Result(
        CameraDiagnostics::ErrorCode::unknown);

    QAtomicInt m_initializationAttemptCount;
    //!map<key, <value, isDirty>>
    std::map<QString, LocalPropertyValue> m_locallySavedProperties;
    std::atomic<bool> m_initInProgress{false};
    QnCommonModule* m_commonModule;
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
