#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "resource/resource.h"



class CLDeviceDirectoryBrowser : public QThread
{
public:
	CLDeviceDirectoryBrowser();
	virtual ~CLDeviceDirectoryBrowser();

    void setDirList(QStringList& dirs);
    QnResourceList result();

protected:
    void run();
    QnResourceList findDevices(const QString& directory);
private:
    static QStringList subDirList(const QString& abspath);
protected:

    QStringList mDirsToCheck;
    QnResourceList mResult;

    volatile bool mNeedStop;
	
};

#endif //directory_browser_h_1708
