#ifndef device_manager_h_1537_
#define device_manager_h_1537_
#include "resourcecontrol/asynch_seacher.h"
#include "resource/resource.h"
#include "resource_criteria.h"
#include "directory_browser.h"



// this class holds all devices in the system ( a least knows how to get the list )
// it also can give list of devices based on some criteria

class QnResource;
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

	QnResourceList getDeviceList(const CLDeviceCriteria& cr);
	QnResource* getDeviceById(QString id);

    CLNetworkDevice* getDeviceByIp(const QHostAddress& ip);

	QnResourceList getRecorderList();
	QnResource* getRecorderById(QString id); 

    void pleaseCheckDirs(const QStringList& lst);
    QStringList getPleaseCheckDirs() const;

    void addFiles(const QStringList& files);
protected:
	CLDeviceManager();
	void onNewDevices_helper(QnResourceList devices, QString parentId);

	bool isDeviceMeetCriteria(const CLDeviceCriteria& cr, QnResource* dev) const;

    bool match_subfilter(QString dec, QString fltr) const;

    void getResultFromDirBrowser();

protected slots:
	void onTimer();

protected:
	QTimer m_timer;
	CLDeviceSearcher m_dev_searcher;
	bool m_firstTime;

	QnResourceList mRecDevices;

    mutable QMutex mPleaseCheckDirsLst_cs;
    QStringList  mPleaseCheckDirsLst;

    CLDeviceDirectoryBrowser mDirbrowsr;

    bool mNeedresultsFromDirbrowsr;
};

#endif //device_manager_h_1537_
