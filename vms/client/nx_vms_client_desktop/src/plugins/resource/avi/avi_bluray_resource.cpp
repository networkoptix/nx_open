#include "avi_bluray_resource.h"

#include <QtCore/QDir>

#include <nx/streaming/archive_stream_reader.h>

#include "avi_bluray_archive_delegate.h"

QnAviBlurayResource::QnAviBlurayResource(const QString& file):
    QnAviResource(file, nullptr)
{
}

QnAviBlurayResource::~QnAviBlurayResource()
{
}

QnAviArchiveDelegate* QnAviBlurayResource::createArchiveDelegate() const
{
    return new QnAVIBlurayArchiveDelegate();
}

bool QnAviBlurayResource::isAcceptedUrl(const QString& url)
{
    QDir sourceDir = url;
    QFileInfoList rootDirList = sourceDir.entryInfoList();
    foreach (const QFileInfo &fi, rootDirList)
    {
        if (fi.isDir() && fi.baseName().toUpper() == QLatin1String("BDMV"))
            sourceDir = QnAVIPlaylistArchiveDelegate::addDirPath(url, fi.baseName());
        else if (fi.isDir() && fi.baseName().toUpper() == QLatin1String("AVCHD"))
            sourceDir = QnAVIPlaylistArchiveDelegate::addDirPath(url, fi.baseName());
    }
    rootDirList = sourceDir.entryInfoList();
    bool playListFound = false;
    bool streamFound = false;
    bool clipInfFound = false;
    foreach (const QFileInfo &fi, rootDirList)
    {
        playListFound |= fi.isDir() && fi.baseName().toUpper() == QLatin1String("PLAYLIST");
        streamFound |= fi.isDir() && fi.baseName().toUpper() == QLatin1String("STREAM");
        clipInfFound |= fi.isDir() && fi.baseName().toUpper() == QLatin1String("CLIPINF");
    }
    bool rez = playListFound && streamFound && clipInfFound;
    if (!rez)
        return false;

    QDir playlistDir = QDir(QnAVIPlaylistArchiveDelegate::addDirPath(sourceDir.absolutePath(), QLatin1String("PLAYLIST")));
    playlistDir.setNameFilters(QStringList() << QLatin1String("*.mpls") << QLatin1String("*.MPLS") << QLatin1String("*.mpl") << QLatin1String("*.MPL"));
    QFileInfoList mplsFileList = playlistDir.entryInfoList();
    return mplsFileList.size() > 0;
}
