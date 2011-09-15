#ifndef directory_browser_h_1708
#define directory_browser_h_1708

#include "deviceserver.h"

class CLDeviceDirectoryBrowser : public QThread
{
Q_OBJECT

public:
    CLDeviceDirectoryBrowser();
    virtual ~CLDeviceDirectoryBrowser();

    void setDirList(QStringList& dirs);
    QStringList directoryList() const { return mDirsToCheck; }
    CLDeviceList result() const;

signals:
    void reload();

protected:
    void run();

private:
    mutable QMutex m_resultMutex;

    QFileSystemWatcher m_fsWatcher;

    QStringList mDirsToCheck;
    CLDeviceList m_tmpResult;
    CLDeviceList mResult;

    volatile bool mNeedStop;

private:
    CLDeviceList findDevices(const QString& directory);

private slots:
    void directoryChanged(const QString & path);
    void reloadDirs();
};

#endif //directory_browser_h_1708
