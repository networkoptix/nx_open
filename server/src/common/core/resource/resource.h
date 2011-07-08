#ifndef device_h_1051
#define device_h_1051


#include "resource_param.h"
#include "datapacket/datapacket.h"
#include "resourcecontrol/resource_command_consumer.h"
#include "id.h"


class QnResourceCommand;
class QnResourceConsumer;

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
        media = 0x08, // video audio 
        playback = 0x10 // something playable ( not real time and not a single shot)
    };

	QnResource();
	virtual ~QnResource();

    //returns true if resources are equal to each other
    virtual bool equalsTo(const QnResourcePtr other) const = 0;

    // flags like network media and so on
	bool checkFlag(unsigned long flag) const;

    QnId getId() const;
    void setId(const QnId& id);

	void setParentId(const QnId& parent);
	QnId getParentId() const;

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
	virtual bool getBasicInfo(){return true;};

	QnParamList& getResourceParamList();// returns params that can be changed on device level
	const QnParamList& getResourceParamList() const;


    void addConsumer(QnResourceConsumer* consumer);
    void removeConsumer(QnResourceConsumer* consumer);
    bool hasSuchConsumer(const QnResourceConsumer* consumer) const;
    void disconnectAllConsumers();

signals:
    
    void onParametrChanged(QString paramname, QString value);

public:
	static void startCommandProc() {static_commandProc.start();};
	static void stopCommandProc() {static_commandProc.stop();};
    static void addCommandToProc(QnAbstractDataPacketPtr data) {static_commandProc.putData(data);};
	static int commandProcQueSize() {return static_commandProc.queueSize();}
	static bool commandProcHasSuchResourceInQueue(QnResourcePtr res) {return static_commandProc.hasSuchResourceInQueue(res);}
protected:
	typedef QMap<QString, QnParamList > QnParamLists;
	static QnParamLists static_resourcesParamLists; // list of all supported resources params list

	// this is thread to process commands like setparam
	static QnResourceCommandProcessor static_commandProc;

protected:
    mutable QMutex m_mutex; // resource mutex for everything 

    unsigned long m_deviceTypeFlags;

    QString m_name; // this device model like AV2105 or AV2155dn 

    mutable QnParamList m_deviceParamList;

private:
    QnId m_Id; //+
    QnId m_parentId;
    QStringList m_tags;
    bool m_avalable;

    mutable QMutex m_consumersMtx; 
    QSet<QnResourceConsumer*> m_consumers;
};

// some resource related non member functions 
bool hasEqual(const QnResourceList& lst, const QnResourcePtr res);

#endif
