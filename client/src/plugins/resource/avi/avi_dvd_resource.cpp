#include "avi_dvd_resource.h"

#include <QtCore/QDir>

#include <plugins/resource/archive/archive_stream_reader.h>

#include "avi_dvd_archive_delegate.h"


QnAviDvdResource::QnAviDvdResource(const QString& file):
    QnAviResource(file)
{
}

QnAviDvdResource::~QnAviDvdResource()
{
}

QnAbstractStreamDataProvider* QnAviDvdResource::createDataProviderInternal(Qn::ConnectionRole /*role*/)
{
    QnArchiveStreamReader* result = new QnArchiveStreamReader(toSharedPointer());
    result->setArchiveDelegate(new QnAVIDvdArchiveDelegate());
    return result;
}

bool QnAviDvdResource::isAcceptedUrl(const QString& url)
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

QString QnAviDvdResource::urlToFirstVTS(const QString& url)
{
    int titleNum = 1;
    int titlePos = url.indexOf(QLatin1Char('?'));
    QString rezUrl = url;
    if (titlePos >= 0) {
        QStringList params = url.mid(titlePos+1).split(QLatin1Char('&'));
        for (int i = 0; i < params.size(); ++i) {
            QStringList values = params[i].split(QLatin1Char('='));
            if (values[0] == QLatin1String("title") && values.size() == 2) 
            {
                titleNum = values[1].toInt();
                break;
            }
        }
        rezUrl = url.left(titlePos);
    }
    if (rezUrl.endsWith(QLatin1Char('/')))
        rezUrl = rezUrl.left(rezUrl.length()-1);
    if (!rezUrl.toUpper().endsWith(QLatin1String("VIDEO_TS")))
        rezUrl += QLatin1String("/VIDEO_TS");
    rezUrl += QLatin1Char('/');
    QString titleStr = QString::number(titleNum);
    if (titleStr.length() < 2)
        titleStr = QLatin1String("0") + titleStr;
    return rezUrl + QLatin1String("VTS_") + titleStr + QLatin1String("_1.VOB");
}

