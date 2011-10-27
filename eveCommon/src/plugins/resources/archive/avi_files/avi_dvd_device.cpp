#include "avi_dvd_device.h"
#include "../archive_stream_reader.h"
#include "avi_dvd_archive_delegate.h"

CLAviDvdDevice::CLAviDvdDevice(const QString& file):
    QnAviResource(file)
{
}

CLAviDvdDevice::~CLAviDvdDevice()
{
}

QnAbstractStreamDataProvider* CLAviDvdDevice::createDataProvider(ConnectionRole role)
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(this);
    result->setArchiveDelegate(new QnAVIDvdArchiveDelegate());
    return result;
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
        sourceDir = QnAVIPlaylistArchiveDelegate::addDirPath(sourceDir, QLatin1String("VIDEO_TS"));
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

QString CLAviDvdDevice::urlToFirstVTS(const QString& url)
{
    int titleNum = 1;
    int titlePos = url.indexOf('?');
    QString rezUrl = url;
    if (titlePos >= 0) {
        QStringList params = url.mid(titlePos+1).split('&');
        for (int i = 0; i < params.size(); ++i) {
            QStringList values = params[i].split('=');
            if (values[0] == "title" && values.size() == 2) 
            {
                titleNum = values[1].toInt();
                break;
            }
        }
        rezUrl = url.left(titlePos);
    }
    if (rezUrl.endsWith('/'))
        rezUrl = rezUrl.left(rezUrl.length()-1);
    if (!rezUrl.toUpper().endsWith("VIDEO_TS"))
        rezUrl += "/VIDEO_TS";
    rezUrl += '/';
    QString titleStr = QString::number(titleNum);
    if (titleStr.length() < 2)
        titleStr = QString("0") + titleStr;
    return rezUrl + "VTS_" + titleStr + "_1.VOB";
}
