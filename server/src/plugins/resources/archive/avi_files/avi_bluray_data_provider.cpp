#include "avi_bluray_data_provider.h"
#include "resource/resource.h"
#include "bluray_helper.h"

CLAVIBlurayStreamReader::CLAVIBlurayStreamReader(QnResource* dev):
    CLAVIPlaylistStreamReader(dev)
{
}

CLAVIBlurayStreamReader::~CLAVIBlurayStreamReader()
{
}

QStringList CLAVIBlurayStreamReader::getPlaylist()
{
    QStringList rez;

    QDir sourceDir = m_device->getUniqueId();

    QFileInfoList rootDirList = sourceDir.entryInfoList();
    foreach(QFileInfo fi, rootDirList)
    {
        if (fi.isDir() && (fi.baseName().toUpper() == "BDMV" || fi.baseName().toUpper() == "AVCHD"))
            sourceDir.cd(fi.baseName());
    }

    QDir playlistDir = sourceDir;
    QDir mediaDir = sourceDir;
    if (sourceDir.absolutePath().endsWith("PLAYLIST"))
        mediaDir.cdUp();
    else 
        playlistDir.cd("PLAYLIST");
    mediaDir.cd("STREAM");

    if (!playlistDir.exists())
        return rez;
    playlistDir.setNameFilters(QStringList() << "*.mpls" << "*.MPLS" << "*.mpl" << "*.MPL");
    QFileInfoList mplsFileList = playlistDir.entryInfoList();

    // finding longest playlist
    double maxDuration = 0; 
    foreach(QFileInfo fi, mplsFileList)
    {
        MPLSParser parser;
        if (!parser.parse(fi.absoluteFilePath()))
            continue;
        QStringList tmpList;
        double playListDuration = 0;
        foreach(MPLSPlayItem playItem, parser.m_playItems)        
        {
            tmpList << mediaDir.absolutePath() + '/' + playItem.m_fileName + '.' + parser.m_fileExt;
            playListDuration += (playItem.m_OUT_time - playItem.m_IN_time)  / (float) MPLSParser::TIME_BASE;
        }
        if (playListDuration >= maxDuration)
        {
            rez = tmpList;
            maxDuration = playListDuration;
        }
    }
    return rez;
}
