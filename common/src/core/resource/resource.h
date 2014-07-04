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

#include <core/datapacket/abstract_data_packet.h>
#include <core/ptz/ptz_fwd.h>

#include <common/common_globals.h>

#include "shared_resource_pointer.h"
#include "resource_fwd.h"
#include "resource_type.h"
#include "param.h"
#include "resource_command_processor.h"

class QnAbstractStreamDataProvider;
class QnResourceConsumer;
class QnResourcePool;

class QnInitResPool: public QThreadPool
{
public:
    QnInitResPool() : QThreadPool() 
    {
        setMaxThreadCount(64);
    }
};

class QN_EXPORT QnResource : public QObject, public QnFromThisToShared<QnResource>
{
    Q_OBJECT
    Q_FLAGS(Flags Flag Qn::PtzCapabilities)
    Q_ENUMS(ConnectionRole Status)
    Q_PROPERTY(QnId id READ getId WRITE setId)
    Q_PROPERTY(QnId typeId READ getTypeId WRITE setTypeId)
    Q_PROPERTY(QString uniqueId READ getUniqueId)
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString searchString READ toSearchString)
    Q_PROPERTY(QnId parentId READ getParentId WRITE setParentId)
    Q_PROPERTY(Status status READ getStatus WRITE setStatus)
    Q_PROPERTY(Flags flags READ flags WRITE setFlags)
    Q_PROPERTY(QString url READ getUrl WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QDateTime lastDiscoveredTime READ getLastDiscoveredTime WRITE setLastDiscoveredTime)
    Q_PROPERTY(QStringList tags READ getTags WRITE setTags)
    Q_PROPERTY(Qn::PtzCapabilities ptzCapabilities READ getPtzCapabilities WRITE setPtzCapabilities)


public:
    // TODO: #Elric #enum
    enum ConnectionRole { Role_Default, Role_LiveVideo, Role_SecondaryLiveVideo, Role_Archive };

    enum Status {
        Offline,
        Unauthorized,
        Online,
        Recording,
        NotDefined
    };

    enum Flag {
        network = 0x01,         /**< Has ip and mac. */
        url = 0x02,             /**< Has url, e.g. file name. */
        streamprovider = 0x04,
        media = 0x08,

        playback = 0x10,        /**< Something playable (not real time and not a single shot). */
        video = 0x20,
        audio = 0x40,
        live = 0x80,

        still_image = 0x100,    /**< Still image device. */

        local = 0x200,          /**< Local client resource. */
        server = 0x400,         /**< Server resource. */
        remote = 0x800,         /**< Remote (on-server) resource. */

        layout = 0x1000,        /**< Layout resource. */
        user = 0x2000,          /**< User resource. */

        utc = 0x4000,           /**< Resource uses UTC-based timing. */
        periods = 0x8000,       /**< Resource has recorded periods. */

        motion = 0x10000,       /**< Resource has motion */
        sync = 0x20000,         /**< Resource can be used in sync playback mode. */

        foreigner = 0x40000,    /**< Resource belongs to other entity. E.g., camera on another server */
        no_last_gop = 0x80000,  /**< Do not use last GOP for this when stream is opened */
        deprecated = 0x100000,  /**< Resource absent in EC but still used in memory for some reason */

        videowall = 0x200000,           /**< Videowall resource */
        desktop_camera = 0x400000,      /**< Desktop Camera resource */

        local_media = local | media,
        local_layout = local | layout,

        local_server = local | server,
        remote_server = remote | server,
        live_cam = utc | sync | live | media | video | streamprovider, // don't set w/o `local` or `remote` flag
        local_live_cam = live_cam | local | network,
        server_live_cam = live_cam | remote,// | network,
        server_archive = remote | media | video | audio | streamprovider,
        ARCHIVE = url | local | media | video | audio | streamprovider,     /**< Local media file. */
        SINGLE_SHOT = url | local | media | still_image | streamprovider    /**< Local still image file. */
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QnResource();
    virtual ~QnResource();

    QnId getId() const;
    void setId(const QnId& id);

    QnId getParentId() const;
    void setParentId(QnId parent);

    // device unique identifier
    virtual QString getUniqueId() const { return getId().toString(); };
    virtual void setUniqId(const QString& value);


    // TypeId unique string id for resource with SUCH list of params and CLASS
    // in other words TypeId can be used instantiate the right resource
    QnId getTypeId() const;
    void setTypeId(QnId id);
    void setTypeByName(const QString& resTypeName);

    virtual Status getStatus() const;
    virtual void setStatus(Status newStatus, bool silenceMode = false);
    QDateTime getLastStatusUpdateTime() const;

    //!this function is called if resourse changes state from offline to online or so 
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
    //!Returns counter of resiource initialization attempts (every attempt: successfull or not)
    int initializationAttemptCount() const;
    
    // flags like network media and so on
    Flags flags() const;
    inline bool hasFlags(Flags flags) const { return (this->flags() & flags) == flags; }
    void setFlags(Flags flags);
    void addFlags(Flags flags);
    void removeFlags(Flags flags);


    //just a simple resource name
    virtual QString getName() const;
    void setName(const QString& name);


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
    QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);
#endif

    QString getUrl() const;
    virtual void setUrl(const QString &url);

    void addTag(const QString& tag);
    void setTags(const QStringList& tags);
    void removeTag(const QString& tag);
    bool hasTag(const QString& tag) const;
    QStringList getTags() const;

    bool hasConsumer(QnResourceConsumer *consumer) const;
    bool hasUnprocessedCommands() const;
    bool isInitialized() const;

    static void stopAsyncTasks();

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
    QnKvPairList getProperties() const;

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
        \param result true, if param succesfully read, false otherwises
    */
    void asyncParamGetDone(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result) const;
    
    //!Emitted on completion of every async set started with setParamAsync
    /*!
        \param paramValue in case \a result == false, this value cannot be relied on
        \param result true, if param succesfully set, false otherwises
    */
    void asyncParamSetDone(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result);

    void initAsyncFinished(const QnResourcePtr &resource, bool initialized); // TODO: #Elric remove signal


public:
    // this is thread to process commands like setparam
    static void startCommandProc();
    static void stopCommandProc();
    static void addCommandToProc(const QnResourceCommandPtr &command);
    static int commandProcQueueSize();

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
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
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
    QnId m_parentId;

    /** Name of this resource. */
    QString m_name;

    /** Url of this resource, if any. */
    QString m_url; 
private:
    /** Resource pool this this resource belongs to. */
    QnResourcePool *m_resourcePool;

    /** Identifier of this resource. */
    QnId m_id;

    /** Identifier of the type of this resource. */
    QnId m_typeId;

    /** Flags of this resource that determine its type. */
    Flags m_flags;
    

    /** Status of this resource. */
    Status m_status;

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

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResource::Flags);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(QnResource::Status) // TODO: #Elric #EC2 move status out, clean up

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

QN_FUSION_DECLARE_FUNCTIONS(QnResource::Status, (metatype)(lexical))

#endif // QN_RESOURCE_H
