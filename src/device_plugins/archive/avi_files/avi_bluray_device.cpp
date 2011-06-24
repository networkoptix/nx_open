#include "avi_bluray_device.h"
#include "avi_bluray_strem_reader.h"

CLAviBluRayDevice::CLAviBluRayDevice(const QString& file):
    CLAviDevice(file)
{
}

CLAviBluRayDevice::~CLAviBluRayDevice()
{
}

CLStreamreader* CLAviBluRayDevice::getDeviceStreamConnection()
{
    return new CLAVIBlurayStreamReader(this); 
}

bool CLAviBluRayDevice::isAcceptedUrl(const QString& url)
{
    QDir sourceDir = url;
    QFileInfoList rootDirList = sourceDir.entryInfoList();
    foreach(QFileInfo fi, rootDirList)
    {
        if (fi.isDir() && fi.baseName().toUpper() == "BDMV")
            sourceDir = CLAVIPlaylistStreamReader::addDirPath(url, fi.baseName());
        else if (fi.isDir() && fi.baseName().toUpper() == "AVCHD")
            sourceDir = CLAVIPlaylistStreamReader::addDirPath(url, fi.baseName());
    }
    rootDirList = sourceDir.entryInfoList();
    bool playListFound = false;
    bool streamFound = false;
    bool clipInfFound = false;
    foreach(QFileInfo fi, rootDirList)
    {
        playListFound |= fi.isDir() && fi.baseName().toUpper() == "PLAYLIST";
        streamFound |= fi.isDir() && fi.baseName().toUpper() == "STREAM";
        clipInfFound |= fi.isDir() && fi.baseName().toUpper() == "CLIPINF";
    }
    bool rez = playListFound && streamFound && clipInfFound;
    if (!rez)
        return false;

    QDir playlistDir = QDir(CLAVIPlaylistStreamReader::addDirPath(sourceDir.absolutePath(), "PLAYLIST"));
    playlistDir.setNameFilters(QStringList() << "*.mpls" << "*.MPLS" << "*.mpl" << "*.MPL");
    QFileInfoList mplsFileList = playlistDir.entryInfoList();
    return mplsFileList.size() > 0;
}
