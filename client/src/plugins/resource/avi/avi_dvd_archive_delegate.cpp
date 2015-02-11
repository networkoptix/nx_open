#include "avi_dvd_archive_delegate.h"

#include <utils/common/mutex.h>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <utils/common/log.h>

#include "utils/media/dvd_reader/dvd_udf.h"
#include "utils/media/dvd_reader/ifo_read.h"
#include "utils/media/dvd_decrypt/dvdcss.h"
#include "utils/media/pespacket.h"
#include "utils/media/nalUnits.h"

#include "avi_dvd_resource.h"

static const int IO_BLOCK_SIZE = 1024 * 1024;
static const int IO_SKIP_BLOCK_SIZE = 1024 * 1024;
static const int SEEK_DIRECT_SECTOR = 5;
static const qint64 MIN_TITLE_PLAYBACK_TIME = 240 * 1000000ll; // ignore title < 2 minutes

struct dvdLangCode
{
    const char* codeString;
    quint16 code;
};

static qint64 dvdtime_to_time(dvd_time_t *dtime, quint8 still_time )
{
    /* Macro to convert Binary Coded Decimal to Decimal */
#define BCD2D(__x__) (((__x__ & 0xf0) >> 4) * 10 + (__x__ & 0x0f))

    double f_fps, f_ms;
    int64_t i_micro_second = 0;

    if (still_time == 0 || still_time == 0xFF)
    {
        i_micro_second += (int64_t)(BCD2D(dtime->hour)) * 60 * 60 * 1000000;
        i_micro_second += (int64_t)(BCD2D(dtime->minute)) * 60 * 1000000;
        i_micro_second += (int64_t)(BCD2D(dtime->second)) * 1000000;

        switch((dtime->frame_u & 0xc0) >> 6)
        {
        case 1:
            f_fps = 25.0;
            break;
        case 3:
            f_fps = 29.97;
            break;
        default:
            f_fps = 2500.0;
            break;
        }
        f_ms = BCD2D(dtime->frame_u&0x3f) * 1000.0 / f_fps;
        i_micro_second += (qint64)(f_ms * 1000.0);
    }
    else
    {
        i_micro_second = still_time;
        i_micro_second = (qint64)((double)i_micro_second * 1000000.0);
    }

    return i_micro_second;
}

/**
 * Returns true if the pack is a NAV pack.  This check is clearly insufficient,
 * and sometimes we incorrectly think that valid other packs are NAV packs.  I
 * need to make this stronger.
 */
int is_nav_pack( unsigned char *buffer )
{
    return ( buffer[ 41 ] == 0xbf && buffer[ 1027 ] == 0xbf );
}

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
    for (unsigned i = 0; i < sizeof(dvdLangCodes) / sizeof(dvdLangCode); ++i)
    {
        if (dvdLangCodes[i].code == langCode)
            return QString::fromLatin1(dvdLangCodes[i].codeString);
    }
    return QString();
}

struct CellPlaybackInfo
{
    qint64 m_firstDts;
    qint64 m_startOffset; // relative start time
    qint64 m_duration;
    int m_startSector;
    int m_startSectorRelativeOffset;
    int m_lastSector;
    bool m_seamless;
};

struct DvdDecryptInfo
{
    DvdDecryptInfo(): m_dvd_file(0), m_duration(0), m_ifo_handle(0), m_fileSize(0), m_currentCell(0) {}
    dvd_file_t* m_dvd_file;
    qint64 m_duration;
    ifo_handle_t* m_ifo_handle;
    QVector<quint16> m_audioLang;
    QVector<CellPlaybackInfo> m_cellList;
    qint64 m_fileSize;
    int m_currentCell;
};

QnAVIDvdArchiveDelegate::QnAVIDvdArchiveDelegate():
    QnAVIPlaylistArchiveDelegate(),
    m_chapter(-1),
    m_dvdReader(0),
    m_tmpBufferSize(0),
    m_mainIfo(0)
{
    m_flags |= Flag_SlowSource;
    m_tmpBuffer = new quint8[IO_BLOCK_SIZE * 2];
    m_dvdReadBuffer = new quint8[IO_BLOCK_SIZE + DVD_VIDEO_LB_LEN];
    m_dvdAlignedBuffer = (quint8*) DVD_ALIGN(m_dvdReadBuffer);
}

QnAVIDvdArchiveDelegate::~QnAVIDvdArchiveDelegate()
{
    close();
    delete [] m_tmpBuffer;
    delete [] m_dvdReadBuffer;
}

void QnAVIDvdArchiveDelegate::setChapterNum(int chapter)
{
    m_chapter = chapter;
}

QStringList QnAVIDvdArchiveDelegate::getTitleList(const QString& url)
{
    QnAVIDvdArchiveDelegate reader;
    return reader.getPlaylist(url);
}

QStringList QnAVIDvdArchiveDelegate::getPlaylist(const QString& url)
{
    QStringList rez;
    int titleNum = -1;
    if (!m_dvdReader)
    {
        QStringList pathAndParams = url.split(QLatin1Char('?'));
        QString path = pathAndParams[0];
        if (pathAndParams.size() > 1)
        {
            //QUrl url = m_device->getUniqueId();
            QString titleStr = QUrlQuery(QUrl(url).query()).queryItemValue(QLatin1String("title"));
            if (!titleStr.isEmpty())
                titleNum = titleStr.toInt();
        }

        if (path.length() == 3 && path.endsWith(QLatin1String(":/")))
            path = path.left(2); // physical mode access under WIN32 expects path in 2-letter format.
        m_dvdReader = DVDOpen(path.toLatin1().constData());
        if (!m_dvdReader)
            return rez;
    }

    if (m_mainIfo == 0)
        m_mainIfo = ifoOpen(m_dvdReader, 0);

    if (m_mainIfo == 0)
        return rez;

    if (titleNum < 1 || titleNum > m_mainIfo->tt_srpt->nr_of_srpts)
    {
        titleNum = -1;
        NX_LOG(QLatin1String("Invalid titleNum value ignored. Open all titles."), cl_logINFO);
    }

    if (titleNum == -1)
    {
        for (int i = 0; i < m_mainIfo->tt_srpt->nr_of_srpts; ++i)
        {
            quint8 i_ttn = m_mainIfo->tt_srpt->title[i].vts_ttn; // number-1 is valid title?
            int vtsnum = m_mainIfo->tt_srpt->title[i].title_set_nr;
            ifo_handle_t* ifo = ifoOpen(m_dvdReader, vtsnum);
            if (ifo)
            {
                quint16 pgc_id = ifo->vts_ptt_srpt->title[i_ttn - 1].ptt[0].pgcn;
                pgc_t* p_pgc = ifo->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
                qint64 playbackTime = dvdtime_to_time(&p_pgc->playback_time, 0);
                if (playbackTime >= MIN_TITLE_PLAYBACK_TIME)
                    rez << QString::number(i+1);
                ifoClose(ifo);
            }
        }
    }
    else
    {
        rez << QString::number(titleNum);
    }
    return rez;
}

// ------------------------ encrypted DVD IO context reader ---------------------------

struct CLAVIDvdStreamReaderPriv
{
    static qint32 readPacket(void *opaque, quint8* buf, int size)
    {
        QnAVIDvdArchiveDelegate* reader = reinterpret_cast<QnAVIDvdArchiveDelegate*> (opaque);
        return reader->readPacket(buf, size);
    }

    static int64_t seek(void* opaque, int64_t offset, qint32 whence)
    {
        QnAVIDvdArchiveDelegate* reader = reinterpret_cast<QnAVIDvdArchiveDelegate*> (opaque);
        return reader->seek(offset, whence);
    }

    static qint32 writePacket(void* opaque, quint8* buf, int bufSize)
    {
        QnAVIDvdArchiveDelegate* reader = reinterpret_cast<QnAVIDvdArchiveDelegate*> (opaque);
        return reader->writePacket(buf, bufSize);
    }
};

AVIOContext* QnAVIDvdArchiveDelegate::getIOContext()
{
    if (m_ffmpegIOContext == 0)
    {
        m_ioBuffer = (quint8*) av_malloc(32*1024);
        m_ffmpegIOContext = avio_alloc_context(
            m_ioBuffer,
            32*1024,
            0,
            this,
            &CLAVIDvdStreamReaderPriv::readPacket,
            &CLAVIDvdStreamReaderPriv::writePacket,
            &CLAVIDvdStreamReaderPriv::seek);
    }
    return m_ffmpegIOContext;
}

qint64 QnAVIDvdArchiveDelegate::findFirstDts(quint8* buffer, int bufSize)
{
    const quint8* bufferEnd = buffer + bufSize;
    const quint8* curPtr = buffer;
    qint64 rez = 0;
    while (rez == 0 && curPtr < bufferEnd)
    {
        PESPacket* pesPacket = (PESPacket*) curPtr;
        //SYSTEM_START_CODE
        if (pesPacket->startCodeExists())
        {
            /* find matching stream */
            quint8 startcode = pesPacket->m_streamID;
            if (
                //(startcode >= 0xc0 && startcode <= 0xdf) ||
                (startcode >= 0xe0 && startcode <= 0xef)  // is video stream
                //(startcode == 0xbd) || (startcode == 0xfd)
                )
            {
                if (pesPacket->hasDts())
                {
                    rez = pesPacket->getDts();
                    return rez;
                }
                else if (pesPacket->hasPts())
                {
                    rez = pesPacket->getPts();
                    return rez;
                }
            }
        }
         curPtr = NALUnit::findNALWithStartCode(curPtr+2, bufferEnd, false);
    }
    return 0;
};

bool QnAVIDvdArchiveDelegate::switchToFile(int newFileIndex)
{
    QString fileName = m_fileList[newFileIndex]->m_name;
    if (newFileIndex >= m_fileList.size())
        return false;

    m_currentPosition = -1;
    m_tmpBufferSize = 0;
    if (newFileIndex != m_currentFileIndex)
    {
        DvdDecryptInfo* data;
        if (m_fileList[newFileIndex]->opaque == 0)
        {
            data = new DvdDecryptInfo();
            m_fileList[newFileIndex]->opaque = data;

            int titleNum = m_fileList[newFileIndex]->m_name.toInt();

            if (m_mainIfo->tt_srpt->nr_of_srpts < titleNum)
                return false;
            quint8 i_ttn = m_mainIfo->tt_srpt->title[titleNum-1].vts_ttn; // number-1 is valid title?
            int vtsnum = m_mainIfo->tt_srpt->title[titleNum-1].title_set_nr;
            int chapid = 0;

            data->m_dvd_file = DVDOpenFile(m_dvdReader, vtsnum, DVD_READ_TITLE_VOBS);
            if (data->m_dvd_file == 0)
                return false;

            if (data->m_ifo_handle == 0)
            {
                data->m_ifo_handle = ifoOpen(m_dvdReader, vtsnum);
                ifo_handle_t* ifo = data->m_ifo_handle;
                data->m_duration = 0;
                if (ifo)
                {
                    // find lang info
                    for (int k = 0; k < ifo->vtsi_mat->nr_of_vts_audio_streams; ++k)
                    {
                        data->m_audioLang << ifo->vtsi_mat->vts_audio_attr[k].lang_code;
                    }

                    // fill cell map
                    ifo_handle_t* ifo = data->m_ifo_handle;


                    if (ifo->vts_ptt_srpt->nr_of_srpts < i_ttn)
                        return false;
                    quint16 pgc_id = ifo->vts_ptt_srpt->title[i_ttn - 1].ptt[chapid].pgcn;

                    quint16 pgn = ifo->vts_ptt_srpt->title[i_ttn - 1].ptt[chapid].pgn;

                    if (ifo->vts_pgcit->nr_of_pgci_srp < pgc_id)
                        return false;
                    pgc_t* p_pgc = ifo->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
                    if ( p_pgc->nr_of_programs < pgn)
                        return false;

                    qint64 totalSectors = 0;

                    int angle = 0;
                    int start_cell = p_pgc->program_map[ pgn - 1 ] - 1;
                    int next_cell = start_cell;
                    for(int cur_cell = start_cell; next_cell < p_pgc->nr_of_cells; )
                    {
                        cur_cell = next_cell;

                        /* Check if we're entering an angle block. */
                        if( p_pgc->cell_playback[ cur_cell ].block_type == BLOCK_TYPE_ANGLE_BLOCK )
                        {
                                cur_cell += angle;
                                for(int i = 0;; ++i )
                                {
                                    if( p_pgc->cell_playback[ cur_cell + i ].block_mode == BLOCK_MODE_LAST_CELL )
                                    {
                                            next_cell = cur_cell + i + 1;
                                            break;
                                    }
                                }
                        } else {
                            next_cell = cur_cell + 1;
                        }

                        cell_playback_t* cell = p_pgc->cell_playback + cur_cell;
                        CellPlaybackInfo cInfo;
                        cInfo.m_duration = dvdtime_to_time(&cell->playback_time, cell->still_time);
                        data->m_duration += cInfo.m_duration;
                        cInfo.m_startSector = cell->first_sector;
                        cInfo.m_lastSector = cell->last_sector;
                        cInfo.m_startOffset = 0;
                        cInfo.m_seamless = cell->stc_discontinuity; //cell->seamless_play;
                        cInfo.m_startSectorRelativeOffset = 0;
                        if (cur_cell > 0)
                        {
                            const CellPlaybackInfo& prev = data->m_cellList[cur_cell-1];
                            cInfo.m_startOffset = prev.m_startOffset + prev.m_duration;
                            cInfo.m_startSectorRelativeOffset = prev.m_startSector + (prev.m_lastSector - prev.m_startSector + 1);
                        }
                        totalSectors += (cell->last_sector - cell->first_sector) + 1;

                        // determine chapter first PTS
                        unsigned char buf[DVD_VIDEO_LB_LEN*16 ];
                        cInfo.m_firstDts = 0;
                        int readed = DVDReadBlocks(data->m_dvd_file, (int) cell->first_sector, 16, buf );
                        if (readed == 16 && is_nav_pack( buf))
                        {
                            // Parse the contained dsi packet.
                            cInfo.m_firstDts = findFirstDts(buf, sizeof(buf)) * 1000000ll / 90000ll;
                        }
                        data->m_cellList << cInfo;

                    }
                    data->m_fileSize = totalSectors * DVDCSS_BLOCK_SIZE;
                }
            }
            if (data->m_duration == 0)
                return false; // skip file
        }
        m_currentFileIndex = newFileIndex;
        if (m_fileList[newFileIndex]->m_formatContext)
        {
            m_formatContext = m_fileList[m_currentFileIndex]->m_formatContext;
            setAudioChannel(m_selectedAudioChannel);
        }
    }
    return true;
}

qint32 QnAVIDvdArchiveDelegate::readPacket(quint8* buf, int size)
{
    if (m_currentPosition == -1)
        seek(0, SEEK_SET);
    size = qMin(size, IO_BLOCK_SIZE);

    CLFileInfo* fi = m_fileList[m_currentFileIndex];
    DvdDecryptInfo* data = (DvdDecryptInfo*) fi->opaque;
    if (data == 0)
        return -1;

    qint64 currentBlock = m_currentPosition / DVDCSS_BLOCK_SIZE;
    if (data->m_currentCell == -1)
        return 0;
    CellPlaybackInfo* cell =  &data->m_cellList[data->m_currentCell];
    int cellLastBlock = cell->m_lastSector;

    if (currentBlock > cellLastBlock && data->m_currentCell == data->m_cellList.size()-1
        && m_currentFileIndex < m_fileList.size()-1 && !m_inSeek)
    {
        // goto next VTS file
        switchToFile(m_currentFileIndex+1);
        seek(0, SEEK_SET);
    }

    int blocksToRead = qMin(IO_BLOCK_SIZE/DVDCSS_BLOCK_SIZE, int(cellLastBlock - currentBlock + 1));
    if (blocksToRead == 0 && data->m_currentCell < data->m_cellList.size()-1)
    {
        // goto next cell
        ++data->m_currentCell;
        cell = &data->m_cellList[data->m_currentCell];
        cellLastBlock = cell->m_lastSector;
        currentBlock = cell->m_startSector;
        m_currentPosition = currentBlock * DVDCSS_BLOCK_SIZE + (m_currentPosition % DVDCSS_BLOCK_SIZE);
        blocksToRead = qMin(IO_BLOCK_SIZE/DVDCSS_BLOCK_SIZE, int(cellLastBlock - currentBlock + 1));
    }

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
        blocksToRead = qMin(IO_BLOCK_SIZE/DVDCSS_BLOCK_SIZE, int(cellLastBlock - currentBlock + 1));
    }

    int availBytes = qMin(size, m_tmpBufferSize);
    memcpy(buf, m_tmpBuffer, availBytes);
    memmove(m_tmpBuffer, m_tmpBuffer + availBytes, m_tmpBufferSize - availBytes);
    m_tmpBufferSize -= availBytes;
    return availBytes;
}

qint64 QnAVIDvdArchiveDelegate::seek(qint64 offset, qint32 whence)
{
    if (m_currentFileIndex == -1)
        return -1;

    CLFileInfo* fi = m_fileList[m_currentFileIndex];
    DvdDecryptInfo* data = (DvdDecryptInfo*) fi->opaque;
    if (data == 0)
        return -1;
    m_tmpBufferSize = 0;
    if (whence == AVSEEK_SIZE)
        return data->m_fileSize;

    if (whence == SEEK_SET)
        ; // offsetInFile = offset; // SEEK_SET by default
    else if (whence == SEEK_CUR)
        offset += m_currentPosition;
    else if (whence == SEEK_END)
        offset = data->m_fileSize - offset;
    if (offset < 0)
        return -1; // seek failed
    else if (offset >= data->m_fileSize)
        return -1; // seek failed

    m_currentPosition = offset;

    data->m_currentCell = -1;
    int block = offset / DVDCSS_BLOCK_SIZE;
    if (whence != SEEK_DIRECT_SECTOR)
    {
        // recalculate offset to cell offset
        for (int i = 0; i < data->m_cellList.size(); ++i)
        {
            const CellPlaybackInfo& cell = data->m_cellList[i];
            if (cell.m_startSectorRelativeOffset <= block && block <= cell.m_startSectorRelativeOffset + (cell.m_lastSector - cell.m_startSector))
            {
                m_currentPosition = block - cell.m_startSectorRelativeOffset; // position inside cell
                m_currentPosition += cell.m_startSector;
                m_currentPosition *= DVDCSS_BLOCK_SIZE;
                m_currentPosition += offset % DVDCSS_BLOCK_SIZE;
                data->m_currentCell = i;
                break;
            }
        }
    }
    else
    {
        for (int i = 0; i < data->m_cellList.size(); ++i)
        {
            const CellPlaybackInfo& cell = data->m_cellList[i];
            if (cell.m_startSector <= block && block <= cell.m_lastSector)
            {
                data->m_currentCell = i;
                break;
            }
        }
    }
    if (data->m_currentCell == -1)
        return -1;
    else
        return offset;
}

qint32 QnAVIDvdArchiveDelegate::writePacket(quint8* /*buf*/, int /*size*/)
{
    return 0; // not implemented
}

void QnAVIDvdArchiveDelegate::deleteFileInfo(CLFileInfo* fi)
{
    DvdDecryptInfo* info = (DvdDecryptInfo*) fi->opaque;
    if (info && info->m_dvd_file)
        DVDCloseFile(info->m_dvd_file);
    if (info && info->m_ifo_handle)
        ifoClose(info->m_ifo_handle);
    delete info;
    QnAVIPlaylistArchiveDelegate::deleteFileInfo(fi);
}

void QnAVIDvdArchiveDelegate::close()
{

    foreach(CLFileInfo* fi, m_fileList)
        deleteFileInfo(fi);
    m_fileList.clear();
    QnAVIPlaylistArchiveDelegate::close();
    if (m_mainIfo)
        ifoClose(m_mainIfo);
    m_mainIfo = 0;
    DVDClose(m_dvdReader);
    m_dvdReader = 0;
    m_tmpBufferSize = 0;

    return;



}

void QnAVIDvdArchiveDelegate::fillAdditionalInfo(CLFileInfo* fi)
{
    DvdDecryptInfo* info = (DvdDecryptInfo*) fi->opaque;
    if (info == 0)
        return;
    fi->m_formatContext->duration = info->m_duration;

    for (unsigned i = 0; i < fi->m_formatContext->nb_streams; ++i)
    {
        AVStream *avStream = fi->m_formatContext->streams[i];
        if (avStream->id >= 128 && avStream->id < 128 + info->m_audioLang.size())
        {
            QString lang = findLangByCode(info->m_audioLang[avStream->id - 128]);
            if (!lang.isEmpty())
            av_dict_set(&avStream->metadata, "language", lang.toLatin1().constData(), 0);
        }
    }
}

bool QnAVIDvdArchiveDelegate::directSeekToPosition(qint64 pos_mks)
{
    DvdDecryptInfo* info = (DvdDecryptInfo*) m_fileList[m_currentFileIndex]->opaque;
    if (info == 0 || info->m_ifo_handle == 0 || info->m_ifo_handle->vts_tmapt == 0 ||
        info->m_ifo_handle->vts_tmapt->nr_of_tmaps == 0)
    {
        return false;
    }

    vts_tmap_t* tmap = info->m_ifo_handle->vts_tmapt->tmap;
    if (tmap->tmu == 0)
        return false;
    int index = pos_mks / 1000000 / tmap->tmu;
    if (index == 0)
        return seek(0, SEEK_SET);
    --index;
    if (index >= tmap->nr_of_entries)
        return false;

    quint64 sector = tmap->map_ent[index] & 0x8ffffffful; // skip 32-th bit (EOF bit)
    return seek(sector * DVDCSS_BLOCK_SIZE, SEEK_DIRECT_SECTOR);
}

qint64 QnAVIDvdArchiveDelegate::packetTimestamp(AVStream* stream, const AVPacket& packet)
{
    // calc offset inside one file by cell info
    static AVRational r = {1, 1000000};
    qint64 packetTime = av_rescale_q(packet.dts, stream->time_base, r);

    DvdDecryptInfo* info = (DvdDecryptInfo*) m_fileList[m_currentFileIndex]->opaque;
    const CellPlaybackInfo& cell = info->m_cellList[info->m_currentCell];
    packetTime = packetTime - cell.m_firstDts + cell.m_startOffset;

    // calc ofset between files
    return packetTime + m_fileList[m_currentFileIndex]->m_offsetInMks;
}
