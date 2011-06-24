#include <QMutex>

#include "avi_dvd_strem_reader.h"
#include "device/device.h"


CLAVIDvdStreamReader::CLAVIDvdStreamReader(CLDevice* dev): 
    CLAVIPlaylistStreamReader(dev),
    m_chapter(-1)
{

}

CLAVIDvdStreamReader::~CLAVIDvdStreamReader()
{

}

void CLAVIDvdStreamReader::setChapterNum(int chapter)
{
    m_chapter = chapter;
}

QStringList CLAVIDvdStreamReader::getPlaylist()
{
    QStringList rez;
    QString sourceDir = m_device->getUniqueId();
    if (!sourceDir.toUpper().endsWith("VIDEO_TS"))
        sourceDir = CLAVIPlaylistStreamReader::addDirPath(sourceDir, "VIDEO_TS");
    QDir dvdDir = QDir(sourceDir);
    if (!dvdDir.exists())
        return rez;

    QString mask = "VTS_";
    dvdDir.setNameFilters(QStringList() << mask+"*.vob" << mask+"*.VOB");
    dvdDir.setSorting(QDir::Name);
    QFileInfoList tmpFileList = dvdDir.entryInfoList();
    QString chapterStr = QString::number(m_chapter);
    if (m_chapter < 10)
        chapterStr.insert(0, "0");

    for (int i = 0; i < tmpFileList.size(); ++i)
    {
        QStringList vobName = tmpFileList[i].baseName().split("_");
        if ((vobName.size()==3) && (vobName.at(2).left(1)!="0"))
        {
            if (m_chapter == -1 || vobName.at(2) == chapterStr)
                rez << tmpFileList[i].absoluteFilePath();
        }
    }
    return rez;
}
