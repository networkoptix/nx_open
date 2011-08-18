#ifndef resource_h_1051
#define resource_h_1051

#include <QtCore/QDateTime>

#include "resource_param.h"
#include "datapacket/datapacket.h"
#include "id.h"

class QDomElement;

class QnResourceCommand;
class QnResourceConsumer;

class QnResource;
typedef QSharedPointer<QnResource> QnResourcePtr;
typedef QList<QnResourcePtr> QnResourceList;
typedef QString QnResourceTypeId;

enum QnDomain
{
    QnDomainMemory = 1,
    QnDomainDatabase = 2,
    QnDomainPhysical = 4
};


class QnResource : public QObject
{
    Q_OBJECT

public:
    enum
    {
        network = 0x01, // resource has ip and mac
        url = 0x02,  // has url, like file name
        streamprovider = 0x04,
        media = 0x08,
        playback = 0x10, // something playable ( not real time and not a single shot)
        video = 0x20,
        audio = 0x40,
        live = 0x80,
        live_cam = live | video | media | streamprovider
    };


    QnResource();
    virtual ~QnResource();

    //returns true if resources are equal to each other
    virtual bool equalsTo(const QnResourcePtr other) const = 0;

    // flags like network media and so on
    void addFlag(unsigned long flag);
    void removeFlag(unsigned long flag);
    bool checkFlag(unsigned long flag) const;

    QnId getId() const;
    void setId(const QnId& id);

    void setParentId(const QnId& parent);
    QnId getParentId() const;

    // TypeId unique string id for resource with SUCH list of params and CLASS
    // in other words TypeId can be used instantiate the right resource
    QnResourceTypeId getTypeId() const;
    void setTypeId(const QnResourceTypeId& id);


    // this value is updated by discovery process
    QDateTime getLastDiscoveredTime() const;
    void setLastDiscoveredTime(const QDateTime &time);

    // if resource physically removed from system - becomes unavailable
    // we even do not need to try setparam or so
    bool available() const;
    void setAvailable(bool av);

    //Name is class of the devices. like 2105DN; => arecontvision 2 megapixel H.264 day night camera;
    QString getName() const;
    void setName(const QString& name);

    void addTag(const QString& tag);
    void removeTag(const QString& tag);
    bool hasTag(const QString& tag) const;
    QStringList tagList() const;

    virtual QString toString() const;
    virtual QString toSearchString() const;


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
    void setParamAsynch(const QString& name, const QnValue& val, QnDomain domain);

    //=============
    // some time we can find resource, but cannot request additional information from it ( resource has bad ip for example )
    // in this case we need to request additional information later.
    // unknownResource - tels if we need that additional information
    virtual bool unknownResource() const;

    // updateResource requests the additional  information and returns resource with same params but additional info; unknownResource() for returned resource must return false
    virtual QnResourcePtr updateResource() = 0;
    //=============


    // this function must be called before use the resource
    // for example some on some cameras we have to setup sensor geometry and I frame frequency
    virtual void beforeUse() = 0;

    QnParamList& getResourceParamList();// returns params that can be changed on device level
    const QnParamList& getResourceParamList() const;


    void addConsumer(QnResourceConsumer* consumer);
    void removeConsumer(QnResourceConsumer* consumer);
    bool hasSuchConsumer(QnResourceConsumer* consumer) const;
    void disconnectAllConsumers();

protected:
    // should change value in memory domain
    virtual bool getParamPhysical(const QString& name, QnValue& val);

    // should just do physical job( network or so ) do not care about memory domain
    virtual bool setParamPhysical(const QString& name, const QnValue& val);

    // some resources might have special params; to setup such params not usual sceme is used
    // if function returns true setParam does not do usual routine
    virtual bool setSpecialParam(const QString& name, const QnValue& val, QnDomain domain);

Q_SIGNALS:
    void onParameterChanged(const QString &paramname, const QString &value);

public:
    static void startCommandProc();
    static void stopCommandProc();
    static void addCommandToProc(QnAbstractDataPacketPtr data);
    static int commandProcQueSize();
    static bool commandProcHasSuchResourceInQueue(QnResourcePtr res);

    static bool loadDevicesParam(const QString& fileName);

private:
    static bool parseResource(const QDomElement &element);
    static bool parseParam(const QDomElement &element, QnParamList& paramlist);

protected:
    typedef QMap<QnResourceTypeId, QnParamList > QnParamLists;
    static QnParamLists staticResourcesParamLists; // list of all supported resources params list

protected:
    mutable QMutex m_mutex; // resource mutex for everything

    unsigned long m_typeFlags;

    QString m_name; // this resource model like AV2105 or AV2155dn

    mutable QnParamList m_resourceParamList;

private:
    QnId m_Id; //+
    QnId m_parentId;

    QnResourceTypeId m_typeId;

    QStringList m_tags;
    bool m_avalable;

    QDateTime m_lastDiscoveredTime;


    mutable QMutex m_consumersMtx;
    QSet<QnResourceConsumer*> m_consumers;
};

// some resource related non member functions

bool hasEqualResource(const QnResourceList& lst, const QnResourcePtr res);
void getResourcesBasicInfo(QnResourceList& lst, int threads);

#endif //resource_h_1051
