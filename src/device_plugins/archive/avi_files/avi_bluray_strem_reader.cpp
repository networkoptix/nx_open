#include "avi_bluray_strem_reader.h"
#include "avi_bluray_device.h"
#include "bluray_helper.h"

CLAVIBlurayStreamReader::CLAVIBlurayStreamReader(CLDevice* dev):
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
            tmpList << mediaDir.absolutePath() + '/' + playItem.fileName + '.' + parser.fileExt;
            playListDuration += (playItem.OUT_time - playItem.IN_time)  / (float) MPLSParser::TIME_BASE;
        }
        if (playListDuration >= maxDuration)
        {
            rez = tmpList;
            maxDuration = playListDuration;
        }
    }
    return rez;
}
