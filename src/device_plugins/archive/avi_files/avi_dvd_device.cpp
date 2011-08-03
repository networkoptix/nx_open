#include "avi_dvd_device.h"
#include "avi_dvd_strem_reader.h"

CLAviDvdDevice::CLAviDvdDevice(const QString& file):
    CLAviDevice(file)
{
}

CLAviDvdDevice::~CLAviDvdDevice()
{
}

CLStreamreader* CLAviDvdDevice::getDeviceStreamConnection()
{
    return new CLAVIDvdStreamReader(this);
}

bool CLAviDvdDevice::isAcceptedUrl(const QString& url)
{
    QString sourceDir = url;
    int paramsPos = sourceDir.indexOf(QLatin1Char('?'));
    if (paramsPos >= 0)
        sourceDir = sourceDir.left(paramsPos);
    if (!sourceDir.toUpper().endsWith(QLatin1String("VIDEO_TS")))
    {
        if (!sourceDir.endsWith(QLatin1Char('/')) && !sourceDir.endsWith(QLatin1Char('\\')))
            sourceDir += QDir::separator();
        sourceDir = CLAVIPlaylistStreamReader::addDirPath(sourceDir, QLatin1String("VIDEO_TS"));
    }
    QDir dvdDir = QDir(sourceDir);
    if (!dvdDir.exists())
        return false;
    QFileInfoList tmpFileList = dvdDir.entryInfoList();

    bool videoTsFound = false;
    bool validNameFound = false;
    bool vobFound = false;
    bool ifoFound = false;

    foreach (const QFileInfo &fi, tmpFileList)
    {
        QString name = fi.baseName().toUpper();
        videoTsFound |= name == QLatin1String("VIDEO_TS");
        validNameFound |= name.startsWith(QLatin1String("VTS_")) && name.count(QLatin1Char('_')) == 2;
        vobFound |= fi.suffix().toUpper() == QLatin1String("VOB");
        ifoFound |= fi.suffix().toUpper() == QLatin1String("IFO");
    }
    return videoTsFound && validNameFound && vobFound && ifoFound;
}
