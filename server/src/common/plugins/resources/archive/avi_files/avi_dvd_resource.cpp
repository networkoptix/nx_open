#include "avi_dvd_resource.h"
#include "avi_dvd_strem_dataprovider.h"

CLAviDvdDevice::CLAviDvdDevice(const QString& file):
    QnAviResource(file)
{
}

CLAviDvdDevice::~CLAviDvdDevice()
{
}

QnAbstractMediaStreamDataProvider* CLAviDvdDevice::getDeviceStreamConnection()
{
    return new CLAVIDvdStreamReader(this);
}

bool CLAviDvdDevice::isAcceptedUrl(const QString& url)
{
    QString sourceDir = url;
    if (!sourceDir.toUpper().endsWith("VIDEO_TS"))
    {
        if (!sourceDir.endsWith('/') && !sourceDir.endsWith('\\'))
            sourceDir += QDir::separator();
        sourceDir = CLAVIPlaylistStreamReader::addDirPath(sourceDir, "VIDEO_TS");
    }
    QDir dvdDir = QDir(sourceDir);
    if (!dvdDir.exists())
        return false;
    QFileInfoList tmpFileList = dvdDir.entryInfoList();
    
    bool videoTsFound = false;
    bool validNameFound = false;
    bool vobFound = false;
    bool ifoFound = false;

    foreach(QFileInfo fi, tmpFileList)
    {
        QString name = fi.baseName().toUpper();
        videoTsFound |= name == "VIDEO_TS";
        validNameFound |= name.startsWith("VTS_") && name.count('_') == 2;
        vobFound |= fi.suffix().toUpper() == "VOB";
        ifoFound |= fi.suffix().toUpper() == "IFO";
    }
    return videoTsFound && validNameFound && vobFound && ifoFound;
}
