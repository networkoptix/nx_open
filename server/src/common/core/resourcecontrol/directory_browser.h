#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "resource/resource.h"



class CLDeviceDirectoryBrowser : public QThread
{
public:
	CLDeviceDirectoryBrowser();
	virtual ~CLDeviceDirectoryBrowser();

    void setDirList(QStringList& dirs);
    CLDeviceList result();

protected:
    void run();
    CLDeviceList findDevices(const QString& directory);
private:
    static QStringList subDirList(const QString& abspath);
protected:

    QStringList mDirsToCheck;
    CLDeviceList mResult;

    volatile bool mNeedStop;
	
};

#endif //directory_browser_h_1708
