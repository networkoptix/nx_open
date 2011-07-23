#ifndef resource_h_1051
#define resource_h_1051


#include "resource_param.h"
#include "datapacket/datapacket.h"
#include "id.h"


class QnResourceCommand;
class QnResourceConsumer;

class QnResource;
typedef QSharedPointer<QnResource> QnResourcePtr;
typedef QList<QnResourcePtr> QnResourceList;


enum QnDomain
{
    QnDomainMemory = 1,
    QnDomainDatabase = 2,
    QnDomainPhysical = 4
};

//enum {NETWORK = 0x00000001, NVR = 0x00000002, SINGLE_SHOT = 0x00000004, ARCHIVE = 0x00000008, RECORDED = 0x00000010};

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
        audio = 0x40
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

    // this value is updated by discovery process
    QDateTime getLastDiscoveredTime() const;
    void setLastDiscoveredTime(QDateTime time);

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


    bool hasSuchParam(const QString& name) const;

	// return true if no error
	virtual bool getParam(const QString& name, QnValue& val, QnDomain domain = QnDomainMemory);

    // same as getParam is invoked in separate thread.
    // as soon as param changed onParametrChanged signal is emitted 
    void getParamAsynch(const QString& name, QnValue& val, QnDomain domain = QnDomainMemory);


	// return true if no error
	virtual bool setParam(const QString& name, const QnValue& val, QnDomain domain = QnDomainMemory);

	// same as setParam but but returns immediately;
	// this function leads setParam invoke in separate thread. so no need to make it virtual
	void setParamAsynch(const QString& name, const QnValue& val, QnDomain domain = QnDomainMemory);

	// gets resource basic info; may be firmware version or so 
    // return false if not accessible 
	virtual bool getBasicInfo() = 0;

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
    virtual bool getParamPhysical(const QString& name, QnValue& val);
    virtual bool setParamPhysical(const QString& name, const QnValue& val);
signals:
    
    void onParametrChanged(QString paramname, QString value);

public:
	static void startCommandProc();
	static void stopCommandProc();
    static void addCommandToProc(QnAbstractDataPacketPtr data);
	static int commandProcQueSize();
    static bool commandProcHasSuchResourceInQueue(QnResourcePtr res);
protected:
	typedef QMap<QString, QnParamList > QnParamLists;
	static QnParamLists static_resourcesParamLists; // list of all supported resources params list

protected:
    mutable QMutex m_mutex; // resource mutex for everything 

    unsigned long m_typeFlags;

    QString m_name; // this device model like AV2105 or AV2155dn 

    mutable QnParamList m_deviceParamList;

private:
    QnId m_Id; //+
    QnId m_parentId;
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
