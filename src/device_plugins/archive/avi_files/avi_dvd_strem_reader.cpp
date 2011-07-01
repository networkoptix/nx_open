#include <QMutex>
#include <QFileInfo>

#include "avi_dvd_strem_reader.h"
#include "device/device.h"
#include "base/dvd_reader/dvd_reader.h"
#include "base/dvd_reader/dvd_udf.h"
#include "base/dvd_reader/ifo_read.h"
#include "base/dvd_decrypt/dvdcss.h"

static const int IO_BLOCK_SIZE = 32 * 1024;
static const int IO_SKIP_BLOCK_SIZE = 1024 * 1024;

struct dvdLangCode
{
    char* codeString;
    quint16 code;
};

dvdLangCode dvdLangCodes[] =
{
{"Afar", 24929},
{"Abkhazian", 24930},
{"Afrikaans", 24934},
{"Albanian", 29553},
{"Amharic", 24941},
{"Arabic", 24946},
{"Armenian", 26977},
{"Assamese", 24947},
{"Aymara", 24953},
{"Azerbaijani", 24954},
{"Bashkir", 25185},
{"Basque", 25973},
{"Bhutani", 25722},
{"Bihari", 25192},
{"Bislama", 25193},
{"Bengali", 25198},
{"Breton", 25202},
{"Bulgarian", 25191},
{"Burmese", 28025},
{"Byelorussian", 25189},
{"Cambodian", 27501},
{"Catalan", 25441},
{"Chinese", 31336},
{"Corsican", 25455},
{"Czech", 25459},
{"Dansk", 25697},
{"Deutsch", 25701},
{"English", 25966},
{"Esperanto", 25967},
{"Espanol", 25971},
{"Estonian", 25972},
{"Finnish", 26217},
{"Fiji", 26218},
{"Faroese", 26223},
{"Francais", 26226},
{"Frisian", 26233},
{"Galician", 26476},
{"Georgian", 27489},
{"Greek", 25964},
{"Greenlandic", 27500},
{"Guarani", 26478},
{"Gujarati", 26485},
{"Hausa", 26721},
{"Hebrew", 26725},
{"Hebrew", 26999},
{"Hindi", 26729},
{"Hrvatski", 26738},
{"Indonesian", 26980},
{"Indonesian", 26990},
{"Interlingue", 26981},
{"Inupiak", 26987},
{"Irish", 26465},
{"Islenska", 26995},
{"Italiano", 26996},
{"Inuktitut", 26997},
{"Japanese", 27233},
{"Javanese", 27255},
{"Kannada", 27502},
{"Kashmiri", 27507},
{"Kazakh", 27499},
{"Korean", 27503},
{"Kurdish", 27509},
{"Kinyarwanda", 29303},
{"Kirghiz", 27513},
{"Kirundi", 29294},
{"Latin", 27745},
{"Lingala", 27758},
{"Laothian", 27759},
{"Lithuanian", 27764},
{"Latvian", 27766},
{"Macedonian", 28011},
{"Magyar", 26741},
{"Malagasy", 28007},
{"Malay", 28019},
{"Malayalam", 28012},
{"Maltese", 28020},
{"Maori", 28009},
{"Marathi", 28018},
{"Moldavian", 28015},
{"Mongolian", 28014},
{"Nauru", 28257},
{"Nederlands", 28268},
{"Nepali", 28261},
{"Norsk", 28271},
{"Occitan", 28515},
{"Oriya", 28530},
{"Oromo", 28525},
{"Pashto", 28787},
{"Persian", 26209},
{"Polish", 28780},
{"Portugues", 28788},
{"Punjabi", 28769},
{"Quechua", 29045},
{"Rhaeto-Romance", 29293},
{"Romanian", 29295},
{"Russian", 29301},
{"Sangho", 29543},
{"Samoan", 29549},
{"Sanskrit", 29537},
{"Scots", 26468},
{"Serbian", 29554},
{"Serbo-Croatian", 29544},
{"Sesotho", 29556},
{"Setswana", 29806},
{"Shona", 29550},
{"Sinhalese", 29545},
{"Sindhi", 29540},
{"Siswati", 29555},
{"Slovak", 29547},
{"Slovenian", 29548},
{"Somali", 29551},
{"Sundanese", 29557},
{"Svenska", 29558},
{"Swahili", 29559},
{"Tagalog", 29804},
{"Tajik", 29799},
{"Tamil", 29793},
{"Tatar", 29812},
{"Telugu", 29797},
{"Thai", 29800},
{"Tibetan", 25199},
{"Tigrinya", 29801},
{"Tonga", 29807},
{"Tsonga", 29811},
{"Turkish", 29810},
{"Turkmen", 29803},
{"Twi", 29815},
{"Uighur", 30055},
{"Ukrainian", 30059},
{"Urdu", 30066},
{"Uzbek", 30074},
{"Vietnamese", 30057},
{"Volapuk", 30319},
{"Welsh", 25465},
{"Wolof", 30575},
{"Xhosa", 30824},
{"Yiddish", 27241},
{"Yiddish", 31081},
{"Yoruba", 31087},
{"Zhuang", 31329},
{"Zulu", 31349}

};

QString findLangByCode(quint16 langCode)
{
    for (int i = 0; i < sizeof(dvdLangCodes) / sizeof(dvdLangCode); ++i)
    {
        if (dvdLangCodes[i].code == langCode)
            return dvdLangCodes[i].codeString;
    }
    return QString();
}

struct DvdDecryptInfo
{
    DvdDecryptInfo(): m_dvd_file(0), m_duration(0) {}
    dvd_file_t* m_dvd_file;
    qint64 m_duration;
    QVector<quint16> m_audioLang;
};

CLAVIDvdStreamReader::CLAVIDvdStreamReader(CLDevice* dev): 
    CLAVIPlaylistStreamReader(dev),
    m_chapter(-1),
    m_dvdReader(0),
    m_tmpBufferSize(0)
{
    m_tmpBuffer = new quint8[IO_BLOCK_SIZE * 2];
    m_dvdReadBuffer = new quint8[IO_BLOCK_SIZE + DVD_VIDEO_LB_LEN];
    m_dvdAlignedBuffer = (quint8*) DVD_ALIGN(m_dvdReadBuffer);
}

CLAVIDvdStreamReader::~CLAVIDvdStreamReader()
{
    delete [] m_tmpBuffer;
    delete [] m_dvdReadBuffer;
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
        if ((vobName.size()==3) && (vobName.at(2).left(1) == "1"))
        {
            rez << tmpFileList[i].absoluteFilePath();
        }
    }
    return rez;
}

// ------------------------ encrypted DVD IO context reader ---------------------------

struct CLAVIDvdStreamReaderPriv
{
    static qint32 readPacket(void *opaque, quint8* buf, int size)
    {
        CLAVIDvdStreamReader* reader = reinterpret_cast<CLAVIDvdStreamReader*> (opaque);
        return reader->readPacket(buf, size);
    }

    static qint64 seek(void* opaque, qint64 offset, qint32 whence)
    {
        CLAVIDvdStreamReader* reader = reinterpret_cast<CLAVIDvdStreamReader*> (opaque);
        return reader->seek(offset, whence);
    }

    static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
    {
        CLAVIDvdStreamReader* reader = reinterpret_cast<CLAVIDvdStreamReader*> (opaque);
        return reader->writePacket(buf, bufSize);
    }
};

ByteIOContext* CLAVIDvdStreamReader::getIOContext() 
{
    //QMutexLocker global_ffmpeg_locker(&global_ffmpeg_mutex);
    if (m_ffmpegIOContext == 0)
    {
        m_ffmpegIOContext = av_alloc_put_byte(      
            m_ioBuffer,
            IO_BLOCK_SIZE,
            0,
            this,
            &CLAVIDvdStreamReaderPriv::readPacket,
            &CLAVIDvdStreamReaderPriv::writePacket,
            &CLAVIDvdStreamReaderPriv::seek);
    }
    return m_ffmpegIOContext;
}

bool CLAVIDvdStreamReader::switchToFile(int newFileIndex)
{
    QString fileName = m_fileList[newFileIndex]->m_name;
    if (!m_dvdReader)
    {
        QString path = m_device->getUniqueId();
        if (path.length() == 3 && path.endsWith(":/"))
            path = path.left(2); // physical mode access under WIN32 expects path in 2-letter format.
        m_dvdReader = DVDOpen(path.toAscii().constData());
        if (!m_dvdReader)
            return false;
    }
    if (newFileIndex >= m_fileList.size())
        return false;

    m_currentPosition = 0;
    m_tmpBufferSize = 0;
    if (newFileIndex != m_currentFileIndex)
    {
        DvdDecryptInfo* data;
        if (m_fileList[newFileIndex]->opaque == 0)
        {
            data = new DvdDecryptInfo();
            m_fileList[newFileIndex]->opaque = data;

            QFileInfo info(m_fileList[newFileIndex]->m_name);
            int number = 1;
            QStringList parts = info.baseName().split('_');
            if (parts.size() > 1)
                number = parts[1].toInt();

            ifo_handle_t* ifo = ifoOpen(m_dvdReader, number);
            if (ifo)
            {
                if (ifo->vts_tmapt && ifo->vts_tmapt->nr_of_tmaps)
                    data->m_duration = ifo->vts_tmapt->tmap->nr_of_entries * ifo->vts_tmapt->tmap->tmu * 1000000ull;
                // find lang info
                audio_attr_t& audioInfo = ifo->vmgi_mat->vmgm_audio_attr;
                // ifo->vtsi_mat->vts->audio_attr->lang_code // nr_of_vts_audio_streams
                for (int k = 0; k < ifo->vtsi_mat->nr_of_vts_audio_streams; ++k)
                {
                    data->m_audioLang << ifo->vtsi_mat->vts_audio_attr[k].lang_code;
                }
                ifoClose(ifo);
            }
            if (data->m_duration == 0)
                return false; // skip file

            data->m_dvd_file = DVDOpenFile(m_dvdReader, number, DVD_READ_TITLE_VOBS);
            if (data->m_dvd_file == 0)
                return false;

        }
        m_currentFileIndex = newFileIndex;
        if (m_fileList[newFileIndex]->m_formatContext)
        {
            QMutexLocker mutex(&m_cs);
            m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
            initCodecs();
        }
    }
    return true;
}

qint32 CLAVIDvdStreamReader::readPacket(quint8* buf, int size)
{
    size = qMin(size, IO_BLOCK_SIZE);

    CLFileInfo* fi = m_fileList[m_currentFileIndex];
    DvdDecryptInfo* data = (DvdDecryptInfo*) fi->opaque;
    if (data == 0)
        return -1;
    qint64 currentFileSize = DvdGetFileSize(data->m_dvd_file);

    if (m_currentPosition >= currentFileSize && m_currentFileIndex < m_fileList.size()-1)
    {
        switchToFile(m_currentFileIndex+1);
        if (m_fileList[m_currentFileIndex]->m_formatContext)
        {
            QMutexLocker mutex(&m_cs);
            m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
            initCodecs();
        }
    }

    int currentBlock = m_currentPosition / DVDCSS_BLOCK_SIZE;
    int blocksToRead = qMin(IO_BLOCK_SIZE/DVDCSS_BLOCK_SIZE, int(currentFileSize/DVDCSS_BLOCK_SIZE - currentBlock));
    while (m_tmpBufferSize < size && blocksToRead > 0)
    {
        int readed = DVDReadBlocks(data->m_dvd_file, currentBlock, blocksToRead, m_dvdAlignedBuffer);
        if (readed >= 0)
        {
            memcpy(m_tmpBuffer + m_tmpBufferSize, m_dvdAlignedBuffer, readed * DVDCSS_BLOCK_SIZE);
            m_tmpBufferSize += readed * DVDCSS_BLOCK_SIZE;
            int offsetRest = m_currentPosition % DVDCSS_BLOCK_SIZE;
            
            if (offsetRest)
            {
                memmove(m_tmpBuffer, m_tmpBuffer + offsetRest, m_tmpBufferSize - offsetRest);
                m_currentPosition -= offsetRest;
                m_tmpBufferSize -= offsetRest;
            }
            
            m_currentPosition += blocksToRead * DVDCSS_BLOCK_SIZE; // increase position always. It is not a bag! So, skip non readable sectors.
            currentBlock += blocksToRead;
        }
        else {
            m_currentPosition += IO_SKIP_BLOCK_SIZE; // skip non readable data
            currentBlock += IO_SKIP_BLOCK_SIZE/DVDCSS_BLOCK_SIZE;
        }
        blocksToRead = qMin(IO_BLOCK_SIZE/DVDCSS_BLOCK_SIZE, int(currentFileSize/DVDCSS_BLOCK_SIZE - currentBlock));
    }

    int availBytes = qMin(size, m_tmpBufferSize);
    memcpy(buf, m_tmpBuffer, availBytes);
    memmove(m_tmpBuffer, m_tmpBuffer + availBytes, m_tmpBufferSize - availBytes);
    m_tmpBufferSize -= availBytes;
    return availBytes;
}

qint64 CLAVIDvdStreamReader::seek(qint64 offset, qint32 whence)
{
    if (m_currentFileIndex == -1)
        return -1;

    CLFileInfo* fi = m_fileList[m_currentFileIndex];
    DvdDecryptInfo* data = (DvdDecryptInfo*) fi->opaque;
    if (data == 0)
        return -1;
    m_tmpBufferSize = 0;
    if (whence == AVSEEK_SIZE)
        return DvdGetFileSize(data->m_dvd_file);

    if (whence == SEEK_SET)
        ; // offsetInFile = offset; // SEEK_SET by default
    else if (whence == SEEK_CUR)
        offset += m_currentPosition;
    else if (whence == SEEK_END)
        offset = DvdGetFileSize(data->m_dvd_file) - offset;
    if (offset < 0)
        return -1; // seek failed
    else if (offset >= DvdGetFileSize(data->m_dvd_file))
        return -1; // seek failed
    
    m_currentPosition = offset;
    return offset;
    
}


qint32 CLAVIDvdStreamReader::writePacket(quint8* buf, int size)
{
    return 0; // not implemented
}


void CLAVIDvdStreamReader::destroy()
{

    foreach(CLFileInfo* fi, m_fileList)
    {
        DvdDecryptInfo* info = (DvdDecryptInfo*) fi->opaque;
        if (info && info->m_dvd_file)
            DVDCloseFile(info->m_dvd_file);
        delete info;
    }
    DVDClose(m_dvdReader);
    m_dvdReader = 0;
    m_tmpBufferSize = 0;
    CLAVIPlaylistStreamReader::destroy();
}

void CLAVIDvdStreamReader::fillAdditionalInfo(CLFileInfo* fi) 
{
    DvdDecryptInfo* info = (DvdDecryptInfo*) fi->opaque;
    if (info == 0)
        return;
    fi->m_formatContext->duration = info->m_duration;

    
    for (int i = 0; i < fi->m_formatContext->nb_streams; ++i)
    {
        AVStream *avStream = fi->m_formatContext->streams[i];
        if (avStream->id >= 128 && avStream->id < 128 + info->m_audioLang.size())
        {
            QString lang = findLangByCode(info->m_audioLang[avStream->id - 128]);
            if (!lang.isEmpty())
            av_metadata_set2(&avStream->metadata, "language", lang.toAscii().constData(), 0);
        }
    }
    
}
