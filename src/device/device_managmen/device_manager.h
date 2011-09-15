#ifndef device_manager_h_1537_
#define device_manager_h_1537_

// this class holds all devices in the system ( a least knows how to get the list )
// it also can give list of devices based on some criteria
#include "../asynch_seacher.h"
#include "../device.h"
#include "device_criteria.h"
#include "../directory_browser.h"

class CLDevice;
class CLNetworkDevice;
class CLRecorderDevice;


// this class is a buffer. makes searcher to serch cameras and puts it into buffer
class CLDeviceManager : public QObject
{
	Q_OBJECT

private:
	~CLDeviceManager();
	static CLDeviceManager* m_Instance;

public:
	static CLDeviceManager& instance();

	CLDeviceSearcher& getDeviceSearcher(); 

	CLDeviceList getDeviceList(const CLDeviceCriteria& cr);
	CLDevice* getDeviceById(QString id);

    CLNetworkDevice* getDeviceByIp(const QHostAddress& ip);

	CLDeviceList getRecorderList();
	CLDevice* getRecorderById(QString id); 

	CLDevice* getArchiveDevice(QString id);

    void pleaseCheckDirs(const QStringList& lst, bool append = false);
    QStringList getPleaseCheckDirs() const;

    void addFiles(const QStringList& files);
protected:
	CLDeviceManager();
	void onNewDevices_helper(CLDeviceList devices, QString parentId);

	bool isDeviceMeetCriteria(const CLDeviceCriteria& cr, CLDevice* dev) const;

	void addArchiver(QString id);
    bool match_subfilter(CLDevice* dev, QString fltr) const;

    void getResultFromDirBrowser();

protected slots:
	void onTimer();

protected:
	QTimer m_timer;
	CLDeviceSearcher m_dev_searcher;
	bool m_firstTime;

	CLDeviceList mRecDevices;

    mutable QMutex mPleaseCheckDirsLst_cs;
    QStringList  mPleaseCheckDirsLst;

    CLDeviceDirectoryBrowser mDirbrowsr;

    bool mNeedresultsFromDirbrowsr;
};

#endif //device_manager_h_1537_
