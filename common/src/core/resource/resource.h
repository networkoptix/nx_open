#ifndef QN_RESOURCE_H
#define QN_RESOURCE_H

#include <cassert>
#include <QDateTime>
#include <QMap>
#include <QMetaType>
#include <QSet>
#include <QStringList>
#include <QReadWriteLock>
#include <QThreadPool>
#include "utils/common/qnid.h"
#include "core/datapacket/abstract_data_packet.h"
#include "resource_fwd.h"
#include "param.h"
#include "resource_type.h"
#include "shared_resource_pointer.h"
#include "interface/abstract_ptz_controller.h"

class QnAbstractStreamDataProvider;
class QnResourceConsumer;
class QnResourcePool;

class QnResourceParameters: public QMap<QString, QString> {
    typedef QMap<QString, QString> base_type;

public:
    using base_type::operator[];

    QString &operator[](const char *key) {
        return base_type::operator[](QLatin1String(key));
    }
};

class QN_EXPORT QnResource : public QObject
{
    Q_OBJECT
    Q_FLAGS(Flags Flag)
    Q_ENUMS(ConnectionRole Status)
    Q_PROPERTY(QnId id READ getId WRITE setId)
    Q_PROPERTY(QnId typeId READ getTypeId WRITE setTypeId)
    Q_PROPERTY(QString uniqueId READ getUniqueId)
    Q_PROPERTY(QString name READ getName WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString searchString READ toSearchString)
    Q_PROPERTY(QnId parentId READ getParentId WRITE setParentId)
    Q_PROPERTY(Status status READ getStatus WRITE setStatus)
    Q_PROPERTY(bool disabled READ isDisabled WRITE setDisabled)
    Q_PROPERTY(Flags flags READ flags WRITE setFlags)
    Q_PROPERTY(QString url READ getUrl WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QDateTime lastDiscoveredTime READ getLastDiscoveredTime WRITE setLastDiscoveredTime)
    Q_PROPERTY(QStringList tags READ tagList WRITE setTags)

public:
    enum ConnectionRole { Role_Default, Role_LiveVideo, Role_SecondaryLiveVideo, Role_Archive };

    enum Status {
        Offline,
        Unauthorized,
        Online,
        Recording };

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

        foreigner = 0x40000,      /**< Resource belongs to other entity. E.g., camera on another server */

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
    QnResource(const QnResourceParameters &params);
    virtual ~QnResource();

    virtual void deserialize(const QnResourceParameters& parameters);

    QnId getId() const;
    void setId(QnId id);

    QnId getParentId() const;
    void setParentId(QnId parent);

    void setGuid(const QString& guid); // TODO: UUID! 
    QString getGuid() const;

    // device unique identifier
    virtual QString getUniqueId() const = 0;
    virtual void setUniqId(const QString& value);


    // TypeId unique string id for resource with SUCH list of params and CLASS
    // in other words TypeId can be used instantiate the right resource
    QnId getTypeId() const;
    void setTypeId(QnId id);

    bool isDisabled() const;
    void setDisabled(bool disabled = true);

    Status getStatus() const;
    virtual void setStatus(Status newStatus, bool silenceMode = false);
    QDateTime getLastStatusUpdateTime() const;

    // this function is called if resourse changes state from offline to online or so 
    void init();
    void initAsync();
    
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


    QnResourcePtr toSharedPointer() const;
    
    template<class Resource>
    static QnSharedResourcePointer<Resource> toSharedPointer(Resource *resource);

    QnResourcePtr getParentResource() const;

    // ==================================================

    QnParamList getResourceParamList() const; // returns params that can be changed on device level

    bool hasParam(const QString &name) const;

    // return true if no error
    bool getParam(const QString &name, QVariant &val, QnDomain domain);

    // same as getParam is invoked in separate thread.
    // as soon as param changed parameterValueChanged() signal is emitted
    void getParamAsync(const QString &name, QnDomain domain);


    // return true if no error
    virtual bool setParam(const QString &name, const QVariant &val, QnDomain domain);

    // same as setParam but but returns immediately;
    // this function leads setParam invoke in separate thread. so no need to make it virtual
    void setParamAsync(const QString &name, const QVariant &val, QnDomain domain);

    // ==============================================================================

    // some time we can find resource, but cannot request additional information from it ( resource has bad ip for example )
    // in this case we need to request additional information later.
    // unknownResource - tels if we need that additional information
    virtual bool unknownResource() const;

    // updateResource requests the additional  information and returns resource with same params but additional info; unknownResource() for returned resource must return false
    virtual QnResourcePtr updateResource() { return QnResourcePtr(0); }
    //=============

    //QnParamList& getDeviceParamList();// returns params that can be changed on device level
    //const QnParamList& getDeviceParamList() const;
    QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);

    //virtual const CLDeviceVideoLayout* getVideoLayout(QnAbstractStreamDataProvider* reader);

    QString getUrl() const;
    virtual void setUrl(const QString &url);

    void addTag(const QString& tag);
    void setTags(const QStringList& tags);
    void removeTag(const QString& tag);
    bool hasTag(const QString& tag) const;
    QStringList tagList() const;

    bool hasConsumer(QnResourceConsumer *consumer) const;
    bool hasUnprocessedCommands() const;
    bool isInitialized() const;

    virtual QnAbstractPtzController* getPtzController(); // TODO: #VASILENKO: OMG what is THIS doing here???

    static void stopAsyncTasks();
signals:
    void parameterValueChanged(const QnResourcePtr &resource, const QnParam &param);
    void statusChanged(const QnResourcePtr &resource);
    void disabledChanged(const QnResourcePtr &resource);
    void nameChanged(const QnResourcePtr &resource);
    void parentIdChanged(const QnResourcePtr &resource);
    void flagsChanged(const QnResourcePtr &resource);
    void urlChanged(const QnResourcePtr &resource);
    void resourceChanged(const QnResourcePtr &resource);

    //!Emitted on completion of every async get started with getParamAsync
    /*!
        \param paramValue in case \a result == false, this value cannot be relied on
        \param result true, if param succesfully read, false otherwises
    */
    void asyncParamGetDone(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result);
    
    //!Emitted on completion of every async set started with setParamAsync
    /*!
        \param paramValue in case \a result == false, this value cannot be relied on
        \param result true, if param succesfully set, false otherwises
    */
    void asyncParamSetDone(const QnResourcePtr &resource, const QString& paramName, const QVariant& paramValue, bool result);

    void initAsyncFinished(const QnResourcePtr &resource, bool initialized);

    void parameterValueChangedQueued(const QnResourcePtr &resource, const QnParam &param);

public:
    // this is thread to process commands like setparam
    static void startCommandProc();
    static void stopCommandProc();
    static void addCommandToProc(QnAbstractDataPacketPtr data);
    static int commandProcQueueSize();

    void update(QnResourcePtr other, bool silenceMode = false);

    // Need use lock/unlock consumers before this call!
    QSet<QnResourceConsumer *> getAllConsumers() const { return m_consumers; }
    void lockConsumers() { m_consumersMtx.lock(); }
    void unlockConsumers() { m_consumersMtx.unlock(); }
protected:
    virtual void updateInner(QnResourcePtr other);

    // should just do physical job ( network or so ) do not care about memory domain
    virtual bool getParamPhysical(const QnParam &param, QVariant &val);
    virtual bool setParamPhysical(const QnParam &param, const QVariant &val);

    virtual bool setSpecialParam(const QString& name, const QVariant& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);

    virtual bool initInternal() {return true;};
    //!Called just after successful \a initInternal()
    /*!
        Inherited class implementation MUST call base class method first
    */
    virtual void initializationDone();

private:
    /* The following consumer-related API is private as it is supposed to be used from QnResourceConsumer instances only.
     * Using it from other places may break invariants. */
    friend class QnResourceConsumer;

    void addConsumer(QnResourceConsumer *consumer);
    void removeConsumer(QnResourceConsumer *consumer);
    void disconnectAllConsumers();
    void initAndEmit();

    void updateUrlName(const QString &oldUrl, const QString &newUrl);

    friend class InitAsyncTask;

private:
    /* Private API for QnSharedResourcePointer. */

    template<class T>
    friend class QnSharedResourcePointer;

    template<class T>
    void initWeakPointer(const QSharedPointer<T> &pointer) {
        assert(!pointer.isNull());
        assert(m_weakPointer.toStrongRef().isNull()); /* Error in this line means that you have created two distinct shared pointers to a single resource instance. */

        m_weakPointer = pointer;
    }

protected:
    /** Mutex that is to be used when accessing a set of all consumers. */
    mutable QMutex m_consumersMtx;

    /** Set of consumers for this resource. */
    QSet<QnResourceConsumer *> m_consumers;

    /** Mutex that is to be used when accessing resource fields. */
    mutable QMutex m_mutex;

    mutable QnParamList m_resourceParamList;

    static bool m_appStopping;
private:
    /** Resource pool this this resource belongs to. */
    QnResourcePool *m_resourcePool;

    /** Weak reference to this, to make conversion to shared pointer possible. */
    QWeakPointer<QnResource> m_weakPointer;

    /** Identifier of this resource. */
    QnId m_id;

    /** Globally unique identifier ot this resource. */
    QString m_guid;

    /** Identifier of the parent resource. Use resource pool to retrieve the actual parent resource. */
    QnId m_parentId;

    /** Identifier of the type of this resource. */
    QnId m_typeId;

    /** Flags of this resource that determine its type. */
    Flags m_flags;
    
    /** Name of this resource. */
    QString m_name;

    /** Disable flag of the resource. */
    bool m_disabled;

    /** Status of this resource. */
    Status m_status;

    /** Url of this resource, if any. */
    QString m_url; 

    QDateTime m_lastDiscoveredTime;
    QDateTime m_lastStatusUpdateTime;

    QStringList m_tags;

    bool m_initialized;    
    QMutex m_initMutex;

    static QThreadPool m_initAsyncPool;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnResource::Flags);

template<class Resource>
QnSharedResourcePointer<Resource> toSharedPointer(Resource *resource) {
    if(resource == NULL) {
        return QnSharedResourcePointer<Resource>();
    } else {
        return resource->toSharedPointer().template dynamicCast<Resource>(); // TODO: replace with staticCast once we deal with virtual inheritance
    }
}

template<class Resource>
QnSharedResourcePointer<Resource> QnResource::toSharedPointer(Resource *resource) {
    using ::toSharedPointer; /* Let ADL kick in. */
    return toSharedPointer(resource);
}


class QnResourceFactory
{
public:
    virtual ~QnResourceFactory() {}

    virtual QnResourcePtr createResource(QnId resourceTypeId, const QnResourceParameters &parameters) = 0;
};


class QnResourceProcessor
{
public:
    virtual ~QnResourceProcessor() {}

    virtual void processResources(const QnResourceList &resources) = 0;
};


// for future use
class QnRecorder : public QnResource
{
};


Q_DECLARE_METATYPE(QnResource::Status);
Q_DECLARE_METATYPE(QnResourcePtr);
Q_DECLARE_METATYPE(QnResourceList);

#endif // QN_RESOURCE_H
