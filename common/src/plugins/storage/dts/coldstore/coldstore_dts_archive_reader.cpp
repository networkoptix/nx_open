#include "coldstore_dts_archive_reader.h"

#ifdef ENABLE_COLDSTORE

#include <utils/common/log.h>

#include <core/resource/resource.h>
#include "core/datapacket/video_data_packet.h"
#include "core/resource/resource_media_layout.h"
#include "../../coldstore/coldstore_api/sfs-client.h"

#include <QtXml/QDomDocument>


#define COLD_STORE_VIDEO_CHANNEL 0
#define COLD_STORE_EVENT_CHANNEL 1 

char delimiter[] = {0,0,0,1}; 
const int del_len = sizeof(delimiter);



extern int contain_subst(char *data, int datalen, char *subdata, int subdatalen);
extern int contain_subst(char *data, int datalen, int start_index ,  char *subdata, int subdatalen);


void getFiles(QList<ColdstoreFile>& files, const QByteArray& indata)
{
    int len = indata.size();
    const char* data = indata.data();
    const char* endpoint = indata.data() + len;


    QString tmp;

    while (data < endpoint )
    {
        ColdstoreFile file;

        if (*data == 0) break;
        tmp = QLatin1String(data);
        data += tmp.length()+1;
        file.channel = tmp.toInt();


        if (*data == 0) break;
        tmp = QLatin1String(data);
        data += tmp.length()+1;
        file.name = tmp;

        if (*data == 0) break;
        tmp = QLatin1String(data);
        data += tmp.length()+1;
        file.original_ms = tmp.toLongLong();
        file.mks = file.original_ms*1000;
        

        file.fps = 15;

        files.push_back(file);

    }
}

//================================================================================
QnColdStoreDelegate::QnColdStoreDelegate(QHostAddress csAddr):
    m_csAddr(csAddr),
    m_opend(false),
    m_loggedError(false),
    m_startTime(0),
    m_endTime(0)
{
    
}

QnColdStoreDelegate::~QnColdStoreDelegate()
{
    close();
}


bool QnColdStoreDelegate::open(const QnResourcePtr &resource)
{

    m_resource = resource;

    //QElapsedTimer t;
    //t.restart();


    m_openedFile = -1;

#ifdef _WIN32
    WSADATA wsaData;
    WORD wVersionRequested = MAKEWORD(1, 1);
    WSAStartup(wVersionRequested, &wsaData);
#endif


    m_csConnection = new Veracity::SFSClient();
    QString ips = m_csAddr.toString();
    QByteArray ipba = ips.toLatin1();
    char* ip = ipba.data();

    if (m_csConnection->Connect(ip) != Veracity::ISFS::STATUS_SUCCESS)
    {
        return false;
    }


    fileListQuery(COLD_STORE_VIDEO_CHANNEL, m_datafileList);
    fileListQuery(COLD_STORE_EVENT_CHANNEL, m_eventfileList);

    


    while(m_datafileList.size()  > m_eventfileList.size())
        m_datafileList.removeLast();

    while(m_eventfileList.size()  > m_datafileList.size())
        m_eventfileList.removeLast();

//    if (m_eventfileList.size() < 2)    try to read last file
    if (m_eventfileList.size() < 1)
    {
        if (m_loggedError == false)
        {
            NX_LOG("Not enough files, only file is still writing", cl_logALWAYS);
            m_loggedError = true;
        }
        return false;
    }

//    NX_LOG("FileQuery time = ", t.elapsed(), cl_logALWAYS);

    // Set start and end times
    m_startTime = m_datafileList.at(0).mks;
    if (!openEventFile(m_datafileList.size()-1))
    {
        NX_LOG("openEventFile failed getting end of time", cl_logALWAYS);
        return false;
    }
    m_endTime = m_eventInfo.back().time;

    // now reset files to initial files
    if (!openEventFile(0))
    {
        NX_LOG("openEventFile failed", cl_logALWAYS);
        return false;
    }

//    NX_LOG("Open1  time = ", t.elapsed(), cl_logALWAYS);

    if (!openCSFile(0, 0))
    {
        NX_LOG("openCSFile failed", cl_logALWAYS);
        return false;
    }


//    NX_LOG("Open2 time = ", t.elapsed(), cl_logALWAYS);    


    m_opend = true;

    return true;

}

void QnColdStoreDelegate::close()
{
    if (m_csConnection)
    {
        if (m_openedFile>=0)
        {
            m_csConnection->Close(m_stream_no);
            m_openedFile=-1;
        }

        m_csConnection->Disconnect();
        delete m_csConnection;
        m_csConnection = 0;
    }

    m_opend = false;
}

qint64 QnColdStoreDelegate::startTime()
{
    return m_startTime;
//    if (!m_opend)
//        return 0;
//
//    return m_datafileList.at(0).mks;
}

qint64 QnColdStoreDelegate::endTime()
{
    return m_endTime;
//    if (!m_opend)
//        return 0;
//
//    return m_datafileList.at(m_datafileList.size()-1).mks;
}

QnAbstractMediaDataPtr QnColdStoreDelegate::getNextData()
{

    if (m_frameInfo.size()==0)
    {
        return QnAbstractMediaDataPtr(0);
    }

    if (m_curr_frame<0 || m_curr_frame > m_frameInfo.size()-1)
    {
        Q_ASSERT(false);
        return QnAbstractMediaDataPtr(0);
    }

    CSFrameInfo fi = m_frameInfo.at(m_curr_frame);

    QnWritableCompressedVideoDataPtr videoData ( new QnWritableCompressedVideoData(CL_MEDIA_ALIGNMENT,fi.lenght) );
    QnByteArray& img = videoData->m_data;

    char* to = img.startWriting(fi.lenght);
    memcpy(to, m_fileContent.data() + fi.shift, fi.lenght);
    img.finishWriting(fi.lenght);

    videoData->compressionType = CODEC_ID_H264 ;
    videoData->width = 0;
    videoData->height = 0;

    videoData->channelNumber = 0;

    videoData->timestamp = fi.time;

    if (fi.i_frame)
        videoData->flags |= QnAbstractMediaData::MediaFlags_AVKey;

    ++m_curr_frame;
    if (m_curr_frame == m_frameInfo.size())
    {
        //reached the end of the event 
        ++m_current_event;
        if (m_current_event == m_eventInfo.size())
        {
            m_current_event = 0;

            ++m_current_file;
//            if (m_current_file == m_datafileList.size()-1) // we never show last file ( minimum we have 2 files )
            if (m_current_file == m_datafileList.size()) // try to read last file
                //reached the end of all files
            {
                m_current_file = 0;
                if (m_openedFile == 0)
                    m_openedFile = 1; // force to reopen a file 
            }

            if (!openEventFile(m_current_file))
            {
                close();
                open(m_resource);
            }

        }

        if (!openCSFile(m_current_file, m_current_event))
        {
            close();
            open(m_resource);
        }

    }

    if (m_curr_frame==m_frameInfo.size())
        Q_ASSERT(false);



    return videoData;

}

qint64 QnColdStoreDelegate::seek(qint64 time, bool findIFrame)
{
    Q_UNUSED(findIFrame);
    int index = find_best_file(time);

    if (index!=m_current_file)
    {
        if (!openEventFile(index))
        {
            close();
            open(m_resource);
        }
    }

    if (m_eventInfo.size()==0)
        return 0;

    m_current_event = find_best_event(time);

    if (openCSFile(m_current_file, m_current_event))
        return m_frameInfo.at(m_curr_frame).time;
    else
    {
        close();
        open(m_resource);
        return startTime();
    }


}

static QSharedPointer<QnDefaultResourceVideoLayout> defaultVideoLayout( new QnDefaultResourceVideoLayout() );
QnResourceVideoLayoutPtr QnColdStoreDelegate::getVideoLayout()
{
    return defaultVideoLayout;
}

static QSharedPointer<QnEmptyResourceAudioLayout> defaultAudioLayout( new QnEmptyResourceAudioLayout() );
QnResourceAudioLayoutPtr QnColdStoreDelegate::getAudioLayout()
{
    return defaultAudioLayout;
}

//===private================================================================

bool QnColdStoreDelegate::fileListQuery(int channel, QList<ColdstoreFile>& lst)
{

    QByteArray filenameBa = m_resource->getUniqueId().toLower().toLatin1();
    filenameBa.replace('-', "");

    char* filename = filenameBa.data();
    char* fullfilename = new char[256];
    strcpy(fullfilename, "TRINITY");
    strcat(fullfilename, filename);

    Veracity::u32 return_results_sizeI;
    Veracity::u32 return_results_countI;
    QByteArray resultBA(1024*1024*20,  0);


    Veracity::u32 status = m_csConnection->FileQuery(
        false,  // distinct; true - do not dublicate resilts
        true, // false ( channel )
        true, // filename
        true, //return_first_ms
        0, // string_start
        0, // string_length
        fullfilename, // 
        channel, // channel
        true, //use_channel
        0, //limit
        false, //order_descending
        0, //time_start
        resultBA.data(),
        &return_results_sizeI, &return_results_countI);

    delete[] fullfilename;
    if (status!=Veracity::ISFS::STATUS_SUCCESS)
        return false;



    getFiles(lst, resultBA);

    

    if (lst.size()<1)
        return false;

    /*
    //normalize time
    quint64 first_ms = lst.at(0).mks;
    QList<ColdstoreFile>::iterator it = lst.begin();
    while(it!=lst.end())
    {
        (*it).mks-= first_ms;
        ++it;
    }
    */

    return true;

}

bool QnColdStoreDelegate::openCSFile(int f_index, int event_index)
{
    m_frameInfo.clear();

    m_curr_frame = 0;
    m_current_file = f_index;
    m_current_event = event_index;


    //QElapsedTimer t;
    //t.restart();    

//    if (f_index > (m_datafileList.size()-2) )
    if (f_index > (m_datafileList.size()-1) )    // Try to read last file
      {
        Q_ASSERT(false);
        return false;
    }

    if (event_index > m_eventInfo.size()-1)
    {
        Q_ASSERT(false);
        return false;
    }

    ColdstoreFile file = m_datafileList.at(f_index);
    


    QByteArray ba = file.name.toLatin1();
    char* cfilename = ba.data();


    Veracity::u64 time_current;
    

    bool need_to_reopen = (m_openedFile!=f_index);

    if (need_to_reopen && m_openedFile>=0)
    {
        m_csConnection->Close(m_stream_no);
        m_openedFile=-1;
    }

    if (need_to_reopen)
    {
        if (m_csConnection->Open(
            cfilename, 
            file.original_ms, file.channel, 
            &m_stream_no, 0, 
            &m_file_size, 
            0,  &time_current) != Veracity::ISFS::STATUS_SUCCESS)
        {
            Q_ASSERT(false);
            return false;
        }

        m_openedFile = f_index;
        m_eventInfo.last().len = m_file_size - m_eventInfo.last().shift; // to complete lst event size 

    }


    CSEvent evnt = m_eventInfo.at(event_index);
    

    //NX_LOG("Open_OPEN time = ", t.elapsed(), cl_logALWAYS);

    if (m_csConnection->Seek(
        m_stream_no, 
        evnt.shift, 
        0, //0 � Do step 1 (file_offset is relative to beginning of file).
        0, //If (seek_whence != 0xff) then this parameter is ignored.
        0, //If (seek_direction == 0xff) then this parameter is ignored
        0xff //seek_direction 0xff � Skip step 2
        ) != Veracity::ISFS::STATUS_SUCCESS)
    {
        //Q_ASSERT(false);

        return false;
    }

    if ((uint)m_fileContent.capacity() < evnt.len)
        m_fileContent.reserve(evnt.len);

    m_fileContent.resize(evnt.len);

    //NX_LOG("Open_SEEK time = ", t.elapsed(), cl_logALWAYS);
    
    Veracity::u64 data_remaining;
    Veracity::u64 returned_data_length;
    Veracity::u32 result = m_csConnection->Read(m_stream_no,
        m_fileContent.data(),
        evnt.len,
        evnt.len,
        &returned_data_length,
        &data_remaining);

    if (result != Veracity::ISFS::STATUS_SUCCESS)
        return false;

    m_fileContent.resize(returned_data_length);
        

    //m_csConnection->Close(m_stream_no);

    //NX_LOG("Open_READ time = ", t.elapsed(), cl_logALWAYS);

    /*/
    FILE* f = fopen("C:/Users/Sergey/Desktop/tests/test.264", "wb");
    fwrite(m_fileContent.data(), m_fileContent.size(), 1, f);
    fclose(f);
    */

    
    char* data = m_fileContent.data();
    int dataLeft = m_fileContent.size();
    int curr_index = 0;
    int shift = 0;    

    int frame_num = 0;
   
    while(1)
    {
        curr_index = contain_subst(data, dataLeft, curr_index + 1, delimiter, del_len);

        if (curr_index < 0 || (dataLeft-curr_index< 4) )
            break;

        char after_start_code = data[curr_index + del_len] & 0xf;

        if (after_start_code != 0x01 && after_start_code != 0x07)
            continue; // not I or P frame


        CSFrameInfo fi;

        fi.lenght = curr_index - shift;
        fi.shift = shift;
        fi.i_frame = (data[shift + 4] & 0xf) == 0x01 ? 0 : 1;
        fi.time = evnt.time + frame_num*(1000*1000/evnt.fps);
        shift = curr_index;

        m_frameInfo.push_back(fi);

        ++frame_num;

    }

    if (m_frameInfo.size()==0)
        return false;

 
//    NX_LOG("==========Open file ", f_index, " event = ", event_index,  cl_logALWAYS);
//    NX_LOG("time ", t.elapsed(),  cl_logALWAYS);

    return true;
}



bool QnColdStoreDelegate::openEventFile(int f_index)
{
    QTime t; t.restart();
    m_frameInfo.clear();
    m_curr_frame = 0;

//    if (f_index > (m_eventfileList.size()-2) )
    if (f_index > (m_eventfileList.size()-1) )   // Try to read last file
    {
        Q_ASSERT(false);
        if (m_loggedError == false)
        {
            NX_LOG("openEventFile not enough files", cl_logALWAYS);
            m_loggedError = true;
        }

        return false;
    }

    ColdstoreFile file = m_eventfileList.at(f_index);

    QByteArray ba = file.name.toLatin1();
    char* cfilename = ba.data();


    Veracity::u32 stream_no;
    Veracity::u64 time_current;


    if (m_csConnection->Open(
        cfilename, 
        file.original_ms, file.channel, 
        &stream_no, 0, 
        &m_file_size, 
        0,  &time_current) != Veracity::ISFS::STATUS_SUCCESS)
    {
        NX_LOG("openEventFile CS file open failed", cl_logALWAYS);
        return false;
    }
    
    m_fileContent.reserve(m_file_size);
    m_fileContent.resize(m_file_size);

    //NX_LOG("OpenEvent_OPEN time = ", t.elapsed(), cl_logALWAYS);

    
    Veracity::u64 data_remaining;
    Veracity::u64 returned_data_length;
    Veracity::u32 result = m_csConnection->Read(stream_no,
        m_fileContent.data(),
        m_file_size,
        m_file_size,
        &returned_data_length,
        &data_remaining);

    if (result != Veracity::ISFS::STATUS_SUCCESS)
    {
        NX_LOG("openEventFile CS file read failed", cl_logALWAYS);
        Q_ASSERT(false);
        return false;
    }

    m_fileContent.resize(returned_data_length);


    m_csConnection->Close(stream_no);

    //NX_LOG("========OpenEvent_READ time = ", t.elapsed(), cl_logALWAYS);

    /*/
    FILE* f = fopen("C:/Users/Patrick/Desktop/coldstore.xml", "wb");
    fwrite(m_fileContent.data(), m_fileContent.size(), 1, f);
    fclose(f);
    /*/

    m_eventInfo.clear();


    QDomDocument doc;

    if (!doc.setContent(m_fileContent))
    {    
        // doc set failed, so try adding closing XML to it and try again    
        m_fileContent = m_fileContent + "</EventList></IQeyeDirectToStorageMetadata>";
    

        if (!doc.setContent(m_fileContent))
        {    
            NX_LOG("openEventFile setContent failed", cl_logALWAYS);
            return false;
        }
    }


    float fps = 15;

    QDomElement root = doc.documentElement();
    if (root.tagName() != QLatin1String("IQeyeDirectToStorageMetadata"))
    {
        NX_LOG("openEventFile root tag find failed", cl_logALWAYS);
        return false;
    }

    QDomNodeList lst = doc.elementsByTagName(QLatin1String("framesPerSecond")); 
    if (lst.size()<1)
    {
        NX_LOG("openEventFile framesPerSecond failed", cl_logALWAYS);
        return false;
    }
    fps = lst.at(0).toElement().text().toFloat();

    lst = doc.elementsByTagName(QLatin1String("Video")); 
    for (int i = 0; i < lst.size(); ++i)
    {
        QDomNode node = lst.at(i);
        QDomElement frameNumberEl = node.firstChildElement(QLatin1String("frameNumber"));
        if (frameNumberEl.isNull())
        {
           NX_LOG("openEventFile frameNumber failed", cl_logALWAYS);
           return false;
        }
        QDomElement fileOffsetEl = node.firstChildElement(QLatin1String("fileOffset"));
        if (fileOffsetEl.isNull())
        {
            NX_LOG("openEventFile fileOffset failed", cl_logALWAYS);
            return false;
        }
        int frameNum = frameNumberEl.text().toInt();
        int fileOffset = fileOffsetEl.text().toInt();

        CSEvent evnt;

        evnt.shift = fileOffset;
        evnt.fps = fps;
        evnt.time = file.mks + frameNum * ( 1000 * 1000 / fps );
        evnt.len = 0;

        m_eventInfo.push_back(evnt);
    }

    QList<CSEvent>::iterator it = m_eventInfo.begin();
    while(1)
    {
        CSEvent& ev_cuur = (*it);

        ++it;
        if (it == m_eventInfo.end())
            break;

        CSEvent& ev_next = (*it);

        ev_cuur.len = ev_next.shift - ev_cuur.shift;
    }

    
    m_current_file = f_index;


//    NX_LOG("openEventFile time = ", t.elapsed(), cl_logALWAYS);


    return true;

}


int QnColdStoreDelegate::find_best_file(quint64 time)
{
    int index = 0;

    while(index < m_datafileList.size() - 1 && time > m_datafileList.at(index+1).mks )
        ++index;

    return index;
}

int QnColdStoreDelegate::find_best_event(quint64 time)
{
    int index = 0;

    while(index < m_eventInfo.size() - 1 && time > m_eventInfo.at(index+1).time)
        ++index;

    return index;
}

//================================================================================

/*
Hi from Germany,

here is the Playlist:

01 - The Extreme - Chillout Worlds Opening
02 - The Extreme - Fantasys
03 - The Extreme - Against Time
04 - The Extreme - Tiefenrausch
05 - The Extreme - Abstrakt Avenue
06 - The Extreme - Matter of Time feat. Shannon Hurley
07 - The Extreme - Sunrise feat. Shannon Hurley
08 - THe Extreme - Invisible Touch
09 - The Extreme - Space Cowboys
10 - The Extreme - Diaspora
11 - The Extreme - Timeless Views
12 - The Extreme - Runaway
13 - The Extreme - Footsteps
14 - The Extreme - In my Dreams
15 - The Extreme - Jetstreams
16 - The Extreme - Once Again
17 - The Extreme - All Tag

This Mix is available as 320K High Quality Version on 16.04.2011 on my Website! 

Cheers
Christian
__________________
The Extreme - Music for Space Travellers
http://www.extreme-chillout.de
http://www.myspace.com/extremechillout

*/

#endif // ENABLE_COLDSTORE

