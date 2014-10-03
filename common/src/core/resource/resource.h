#ifndef QN_RESOURCE_H
#define QN_RESOURCE_H

#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSet>
#include <QtCore/QStringList>
#include <QtCore/QReadWriteLock>
#include <QtCore/QThreadPool>

#include <api/model/kvpair.h>

#include <utils/camera/camera_diagnostics.h>
#include <utils/common/from_this_to_shared.h>
#include <utils/common/model_functions_fwd.h>
#include <utils/common/id.h>

#include <core/ptz/ptz_fwd.h>

#include <common/common_globals.h>
#include "resource_command_processor.h"
#include "shared_resource_pointer.h"
#include "resource_fwd.h"
#include "resource_type.h"
#include "param.h"

class QnAbstractStreamDataProvider;
class QnResourceConsumer;
class QnResourcePool;

class QnInitResPool: public QThreadPool
{
public:
};

class QN_EXPORT QnResource : public QObject, public QnFromThisToShared<QnResource>
{
    Q_OBJECT
    Q_FLAGS(Qn::PtzCapabilities)
    Q_PROPERTY(QnUuid id READ getId WRITE setId)
    Q_PROPERTY(QnUuid typeId READ getTypeId WRITE setTypeId)
    Q_PROPERTY(QString uniqueId READ getUniqueId)
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString searchString READ toSearchString)
    Q_PROPERTY(QnUuid parentId READ getParentId WRITE setParentId)
    Q_PROPERTY(Qn::ResourceStatus status READ getStatus WRITE setStatus)
    Q_PROPERTY(Qn::ResourceFlags flags READ flags WRITE setFlags)
    Q_PROPERTY(QString url READ getUrl WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QDateTime lastDiscoveredTime READ getLastDiscoveredTime WRITE setLastDiscoveredTime)
    Q_PROPERTY(QStringList tags READ getTags WRITE setTags)
    Q_PROPERTY(Qn::PtzCapabilities ptzCapabilities READ getPtzCapabilities WRITE setPtzCapabilities)
public:
    QnResource();
    virtual ~QnResource();

    QnUuid getId() const;
    void setId(const QnUuid& id);

    QnUuid getParentId() const;
    void setParentId(QnUuid parent);

    // device unique identifier
    virtual QString getUniqueId() const { return getId().toString(); };
    virtual void setUniqId(const QString& value);


    // TypeId unique string id for resource with SUCH list of params and CLASS
    // in other words TypeId can be used instantiate the right resource
    QnUuid getTypeId() const;
    void setTypeId(const QnUuid &id);
    void setTypeByName(const QString& resTypeName);

    virtual Qn::ResourceStatus getStatus() const;
    virtual void setStatus(Qn::ResourceStatus newStatus, bool silenceMode = false);
    QDateTime getLastStatusUpdateTime() const;

    //!this function is called if resource changes state from offline to online or so 
    /*!
        \note If \a QnResource::init is already running in another thread, this method exits immediately and returns false
        \return true, if initialization attempt is done (with success or failure). false, if \a QnResource::init is already running in other thread
    */
    bool init();

    void setLastMediaIssue(const CameraDiagnostics::Result& issue);
    CameraDiagnostics::Result getLastMediaIssue() const;

    /*!
        Calls \a QnResource::init. If \a QnResource::init is already running in another thread, this method waits for it to complete
    */
    void blockingInit();
    void initAsync(bool optional);
    CameraDiagnostics::Result prevInitializationResult() const;
    //!Returns counter of resource initialization attempts (every attempt: successful or not)
    int initializationAttemptCount() const;
    
    // flags like network media and so on
    Qn::ResourceFlags flags() const;
    inline bool hasFlags(Qn::ResourceFlags flags) const { return (this->flags() & flags) == flags; }
    void setFlags(Qn::ResourceFlags flags);
    void addFlags(Qn::ResourceFlags flags);
    void removeFlags(Qn::ResourceFlags flags);


    //just a simple resource name
    virtual QString getName() const;
    virtual void setName(const QString& name);


    // this value is updated by discovery process
    QDateTime getLastDiscoveredTime() const;
    void setLastDiscoveredTime(const QDateTime &time);

    QnResourcePool *resourcePool() const;
    void setResourcePool(QnResourcePool *resourcePool);

    virtual QString toString() const;
    virtual QString toSearchString() const;

    
    template<class Resource>
    static QnSharedResourcePointer<Resource> toSharedPointer(Resource *resource);

    QnResourcePtr getParentResource() const;

    // ==================================================

    QnParamList getResourceParamList() const; // returns params that can be changed on device level

    bool hasParam(const QString &name) const;

    // return true if no error
    bool getParam(const QString &name, QVariant &val, QnDomain domain) const;

#ifdef ENABLE_DATA_PROVIDERS
    // same as getParam is invoked in separate thread.
    // as soon as param changed parameterValueChanged() signal is emitted
    void getParamAsync(const QString &name, QnDomain domain);
#endif 


    // return true if no error
    virtual bool setParam(const QString &name, const QVariant &val, QnDomain domain);

#ifdef ENABLE_DATA_PROVIDERS
    // same as setParam but but returns immediately;
    // this function leads setParam invoke in separate thread. so no need to make it virtual
    void setParamAsync(const QString &name, const QVariant &val, QnDomain domain);
#endif 

    // some time we can find resource, but cannot request additional information from it ( resource has bad ip for example )
    // in this case we need to request additional information later.
    // unknownResource - tels if we need that additional information
    virtual bool unknownResource() const;

    // updateResource requests the additional  information and returns resource with same params but additional info; unknownResource() for returned resource must return false
    virtual QnResourcePtr updateResource() { return QnResourcePtr(0); }

#ifdef ENABLE_DATA_PROVIDERS
    QnAbstractStreamDataProvider* createDataProvider(Qn::ConnectionRole role);
#endif

    QString getUrl() const;
    virtual void setUrl(const QString &url);

    void addTag(const QString& tag);
    void setTags(const QStringList& tags);
    void removeTag(const QString& tag);
    bool hasTag(const QString& tag) const;
    QStringList getTags() const;

    bool hasConsumer(QnResourceConsumer *consumer) const;
#ifdef ENABLE_DATA_PROVIDERS
    bool hasUnprocessedCommands() const;
#endif

    bool isInitialized() const;

    static void stopAsyncTasks();
    static void pleaseStopAsyncTasks();

    /**
        Control PTZ flags. Better place is mediaResource but no signals allowed in MediaResource
    */
    Qn::PtzCapabilities getPtzCapabilities() const;
    bool hasPtzCapabilities(Qn::PtzCapabilities capabilities) const;
    void setPtzCapabilities(Qn::PtzCapabilities capabilities);
    void setPtzCapability(Qn::PtzCapabilities capability, bool value);
    QnAbstractPtzController *createPtzController(); // TODO: #Elric does not belong here

    /* Note that these functions hide property API inherited from QObject.
     * This is intended as this API cannot be used with QnResource anyway 
     * because of threading issues. */

    bool hasProperty(const QString &key) const;
    QString getProperty(const QString &key, const QString &defaultValue = QString()) const;
    void setProperty(const QString &key, const QString &value);
    ec2::ApiResourceParamDataList getProperties() const;

    static QnInitResPool* initAsyncPoolInstance();

signals:
    void parameterValueChanged(const QnResourcePtr &resource, const QnParam &param) const;
    void statusChanged(const QnResourcePtr &resource);
    void nameChanged(const QnResourcePtr &resource);
    void parentIdChanged(const QnResourcePtr &resource);
    void flagsChanged(const QnResourcePtr &resource);
    void urlChanged(const QnResourcePtr &resource);
    void resourceChanged(const QnResourcePtr &resource);
    void ptzCapabilitiesChanged(const QnResourcePtr &resource);
    void mediaDewarpingParamsChanged(const QnResourcePtr &resource);
    void propertyChanged(const QnResourcePtr &resource, const QString &key);
    void initializedChanged(const QnResourcePtr &resource);
    void videoLayoutChanged(const QnResourcePtr &resource);

    //!Emitted on completion of every async get started with getParamAsync
    /*!
        \param paramValue in case \a result == false, this value cannot be relied on
        \param result true, if param successfully read, false otherwise
    */
    void asyncParamGetDone(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result) const;
    
    //!Emitted on completion of every async set started with setParamAsync
    /*!
        \param paramValue in case \a result == false, this value cannot be relied on
        \param result true, if param successfully set, false otherwise
    */
    void asyncParamSetDone(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result);

    void initAsyncFinished(const QnResourcePtr &resource, bool initialized); // TODO: #Elric remove signal


public:
#ifdef ENABLE_DATA_PROVIDERS
    // this is thread to process commands like setparam
    static void startCommandProc();
    static void stopCommandProc();
    static void addCommandToProc(const QSharedPointer<QnResourceCommand> &command);
    static int commandProcQueueSize();
#endif

    void update(const QnResourcePtr& other, bool silenceMode = false);

    // Need use lock/unlock consumers before this call!
    QSet<QnResourceConsumer *> getAllConsumers() const { return m_consumers; }
    void lockConsumers() { m_consumersMtx.lock(); }
    void unlockConsumers() { m_consumersMtx.unlock(); }

    QnResourcePtr toSharedPointer() const;

protected:
    virtual void updateInner(const QnResourcePtr &other, QSet<QByteArray>& modifiedFields);

    // should just do physical job ( network or so ) do not care about memory domain
    virtual bool getParamPhysical(const QnParam &param, QVariant &val);
    virtual bool setParamPhysical(const QnParam &param, const QVariant &val);

    virtual bool setSpecialParam(const QString& name, const QVariant& val, QnDomain domain);

#ifdef ENABLE_DATA_PROVIDERS
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(Qn::ConnectionRole role);
#endif

    virtual QnAbstractPtzController *createPtzControllerInternal(); // TODO: #Elric does not belong here

    virtual CameraDiagnostics::Result initInternal() {return CameraDiagnostics::NoErrorResult();};
    //!Called just after successful \a initInternal()
    /*!
        Inherited class implementation MUST call base class method first
    */
    virtual void initializationDone();

    virtual void parameterValueChangedNotify(const QnParam &param);
private:
    /* The following consumer-related API is private as it is supposed to be used from QnResourceConsumer instances only.
     * Using it from other places may break invariants. */
    friend class QnResourceConsumer;

    void addConsumer(QnResourceConsumer *consumer);
    void removeConsumer(QnResourceConsumer *consumer);
    void disconnectAllConsumers();
    void initAndEmit();

    void updateUrlName(const QString &oldUrl, const QString &newUrl);
    bool emitDynamicSignal(const char *signal, void **arguments);
    void afterUpdateInner(QSet<QByteArray>& modifiedFields);

    friend class InitAsyncTask;

protected:
    /** Mutex that is to be used when accessing a set of all consumers. */
    mutable QMutex m_consumersMtx;

    /** Set of consumers for this resource. */
    QSet<QnResourceConsumer *> m_consumers;

    /** Mutex that is to be used when accessing resource fields. */
    mutable QMutex m_mutex;
    QMutex m_initMutex;

    mutable QnParamList m_resourceParamList;

    static bool m_appStopping;

    /** Identifier of the parent resource. Use resource pool to retrieve the actual parent resource. */
    QnUuid m_parentId;

    /** Name of this resource. */
    QString m_name;

    /** Url of this resource, if any. */
    QString m_url; 
private:
    /** Resource pool this this resource belongs to. */
    QnResourcePool *m_resourcePool;

    /** Identifier of this resource. */
    QnUuid m_id;

    /** Identifier of the type of this resource. */
    QnUuid m_typeId;

    /** Flags of this resource that determine its type. */
    Qn::ResourceFlags m_flags;
    

    /** Status of this resource. */
    Qn::ResourceStatus m_status;

    QDateTime m_lastDiscoveredTime;
    QDateTime m_lastStatusUpdateTime;

    QStringList m_tags;

    /** Additional values aka kvPairs. */
    QHash<QString, QString> m_propertyByKey;

    bool m_initialized;    
    QMutex m_initAsyncMutex;

    static QnInitResPool m_initAsyncPool;
    qint64 m_lastInitTime;
    CameraDiagnostics::Result m_prevInitializationResult;
    CameraDiagnostics::Result m_lastMediaIssue;
    QAtomicInt m_initializationAttemptCount;
};

template<class Resource>
QnSharedResourcePointer<Resource> toSharedPointer(Resource *resource) {
    if(resource == NULL) {
        return QnSharedResourcePointer<Resource>();
    } else {
        return resource->toSharedPointer().template staticCast<Resource>();
    }
}

template<class Resource>
QnSharedResourcePointer<Resource> QnResource::toSharedPointer(Resource *resource) {
    using ::toSharedPointer; /* Let ADL kick in. */
    return toSharedPointer(resource);
}

Q_DECLARE_METATYPE(QnResourcePtr);
Q_DECLARE_METATYPE(QnResourceList);

#endif // QN_RESOURCE_H
