#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include <QtCore/QDateTime>
#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QSet>
#include <QtCore/QSharedPointer>
#include <QtCore/QStringList>

#include "param.h"
#include "resource_type.h"
#include "utils/common/qnid.h"
#include "../datapacket/datapacket.h"

class QnAbstractStreamDataProvider;
class QnResourceConsumer;

// this class and inherited must be very light to create
class QnResource;
typedef QSharedPointer<QnResource> QnResourcePtr;
typedef QList<QnResourcePtr> QnResourceList;
typedef QMap<QString, QnResourcePtr> QnResourceMap;

enum QN_EXPORT QnDomain
{
    QnDomainMemory = 1,
    QnDomainDatabase = 2,
    QnDomainPhysical = 4
};

typedef QMap<QString, QString> QnResourceParameters;

class QN_EXPORT QnResource : public QObject //: public CLRefCounter
{
    Q_OBJECT
    Q_PROPERTY(QString name READ getName WRITE setName USER false) // do not show at GUI
    Q_PROPERTY(QString url READ getUrl WRITE setUrl)

public:
    enum ConnectionRole { Role_Default, Role_LiveVideo, Role_Archive };

    enum Status { Online, Offline };

    enum
    {
        network = 0x01, // resource has ip and mac
        url = 0x02,  // has url, like file name
        streamprovider = 0x04,
        media = 0x08,
        playback = 0x10, // something playable (not real time and not a single shot)
        video = 0x20,
        audio = 0x40,
        live = 0x80,
        still_image = 0x100, // still image device

        local = 0x200,    // local client resource
        server = 0x400,   // server resource
        remote = 0x800,   // remote (on-server) resource

        live_cam = live | media | video | streamprovider, // don't set w/o `local` or `remote` flag
        local_live_cam = live_cam | local | network,
        server_live_cam = live_cam | remote,// | network,
        server_archive = remote | media | video | audio | streamprovider,
        ARCHIVE = url | local | media | video | audio | streamprovider,     // local media file
        SINGLE_SHOT = url | local | media | still_image | streamprovider    // local still image file
    };

    QnResource();
    QnResource(const QnResourceParameters &params);
    virtual ~QnResource();

    virtual void deserialize(const QnResourceParameters& parameters);

    QnId getId() const;
    void setId(const QnId& id);

    QnId getParentId() const;
    void setParentId(const QnId& parent);

    // device unique identifier
    virtual QString getUniqueId() const = 0;


    // TypeId unique string id for resource with SUCH list of params and CLASS
    // in other words TypeId can be used instantiate the right resource
    QnId getTypeId() const;
    void setTypeId(const QnId& id);

    Status getStatus() const;
    void setStatus(Status status);


    // flags like network media and so on
    unsigned long flags() const;
    bool checkFlag(unsigned long flag) const;
    void setFlags(unsigned long flags);
    void addFlag(unsigned long flag);
    void removeFlag(unsigned long flag);


    //just a simple resource name
    virtual QString getName() const;
    void setName(const QString& name);


    // this value is updated by discovery process
    QDateTime getLastDiscoveredTime() const;
    void setLastDiscoveredTime(const QDateTime &time);


    virtual QString toString() const;
    virtual QString toSearchString() const;

    // ==================================================

    QnParamList& getResourceParamList() const;// returns params that can be changed on device level

    bool hasSuchParam(const QString& name) const;

    // return true if no error
    virtual bool getParam(const QString& name, QnValue& val, QnDomain domain);

    // same as getParam is invoked in separate thread.
    // as soon as param changed onParameterChanged signal is emitted
    void getParamAsynch(const QString& name, QnValue& val, QnDomain domain);


    // return true if no error
    virtual bool setParam(const QString& name, const QnValue& val, QnDomain domain);

    // same as setParam but but returns immediately;
    // this function leads setParam invoke in separate thread. so no need to make it virtual
    void setParamAsynch(const QString& name, const QnValue& val, QnDomain domain );

    // ==============================================================================

    // some time we can find resource, but cannot request additional information from it ( resource has bad ip for example )
    // in this case we need to request additional information later.
    // unknownResource - tels if we need that additional information
    virtual bool unknownResource() const;

    // updateResource requests the additional  information and returns resource with same params but additional info; unknownResource() for returned resource must return false
    virtual QnResourcePtr updateResource() { return QnResourcePtr(0); }
    //=============

    // this function is called by stream reader before start read;
    // on in case of connection lost and restored
    virtual void beforeUse() {}


    //QnParamList& getDeviceParamList();// returns params that can be changed on device level
    //const QnParamList& getDeviceParamList() const;
    QnAbstractStreamDataProvider* createDataProvider(ConnectionRole role);

    //virtual const CLDeviceVideoLayout* getVideoLayout(QnAbstractStreamDataProvider* reader);

    bool associatedWithFile() const;

    QString getUrl() const;
    void setUrl(const QString& value);

    void addTag(const QString& tag);
    void setTags(const QStringList& tags);
    void removeTag(const QString& tag);
    bool hasTag(const QString& tag) const;
    QStringList tagList() const;

    QnResourcePtr toSharedPointer() const;
    void addConsumer(QnResourceConsumer* consumer);
    void removeConsumer(QnResourceConsumer* consumer);
    bool hasSuchConsumer(QnResourceConsumer* consumer) const;
    void disconnectAllConsumers();

Q_SIGNALS:
    void onParameterChanged(const QString &paramname, const QString &value);
    void onStatusChanged(QnResource::Status oldStatus, QnResource::Status newStatus);

public:
    // this is thread to process commands like setparam
    static void startCommandProc();
    static void stopCommandProc();
    static void addCommandToProc(QnAbstractDataPacketPtr data);
    static int commandProcQueSize();
    static bool commandProchasSuchDeviceInQueue(QnResourcePtr res);

protected:
    // should change value in memory domain
    virtual bool getParamPhysical(const QString& name, QnValue& val);

    // should just do physical job( network or so ) do not care about memory domain
    virtual bool setParamPhysical(const QString& name, const QnValue& val);
    virtual bool setSpecialParam(const QString& name, const QnValue& val, QnDomain domain);

    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role) = 0;

protected:
    //typedef QMap<QnId, QnParamList > QnParamLists; // key - resource type ID
    //static QnParamLists staticResourcesParamLists; // list of all supported resources params list

protected:
    mutable QMutex m_mutex;
    unsigned long m_flags;
    QString m_name;

    mutable QnParamList m_resourceParamList;

private:
    QnId m_id;
    QnId m_parentId;

    QnId m_typeId;

    QDateTime m_lastDiscoveredTime;

    QStringList m_tags;

    bool m_avalable;

    QString m_url; //-

    Status m_status;


    mutable QnParamList m_streamParamList; //-
    mutable QMutex m_consumersMtx;
    QSet<QnResourceConsumer*> m_consumers;
};


class QnResourceFactory
{
public:
    virtual QnResourcePtr createResource(const QnId& resourceTypeId, const QnResourceParameters& parameters) = 0;
};


class QnDummyResourceFactory : public QnResourceFactory
{
public:
    QnResourcePtr createResource(const QnId& /*resourceTypeId*/, const QnResourceParameters& /*parameters*/) { return QnResourcePtr(0); }
};


class QnResourceProcessor
{
public:
    virtual void processResources(const QnResourceList& resources) = 0;
};


// for future use
class QnRecorder : public QnResource
{
};

#endif // __RESOURCE_H__
