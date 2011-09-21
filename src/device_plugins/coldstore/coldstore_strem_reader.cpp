#include "coldstore_strem_reader.h"
#include "coldstore_api/sfs-client.h"
#include "coldstore_device.h"

struct ColdstoreFile
{
    QString name;
    quint64 ms;
    int channel;
};

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
        tmp = QString(data);
        data += tmp.length()+1;
        file.channel = tmp.toInt();


        if (*data == 0) break;
        tmp = QString(data);
        data += tmp.length()+1;
        file.name = tmp;

        if (*data == 0) break;
        tmp = QString(data);
        data += tmp.length()+1;
        file.ms = tmp.toLongLong();

        files.push_back(file);

    }
}


ColdStoreStreamReader::ColdStoreStreamReader(CLDevice* dev):
CLAbstractArchiveReader(dev),
m_inited(false),
m_csConnection(0)
{

}

ColdStoreStreamReader::~ColdStoreStreamReader()
{
    destroy();
}


CLAbstractMediaData* ColdStoreStreamReader::getNextData()
{
    if (!m_inited)
    {
        if (init())
            m_inited = true;
        else
            destroy();

    }
    return 0;
}

quint64 ColdStoreStreamReader::currentTime(void) const
{
    return 0;
}

bool ColdStoreStreamReader::isNegativeSpeedSupported() const
{
    return false;
}

void ColdStoreStreamReader::channeljumpTo(quint64,int)
{

}
//=======================================================================
bool ColdStoreStreamReader::init()
{
    m_csConnection = new Veracity::SFSClient();
    ColdStoreDevice* cs_dev = static_cast<ColdStoreDevice*>(getDevice());
    QString ips = cs_dev->getCSAddr().toString();
    QByteArray ipba = ips.toLocal8Bit();
    char* ip = ipba.data();

    if (m_csConnection->Connect(ip) != Veracity::ISFS::STATUS_SUCCESS)
    {
        return false;
    }


    QByteArray filenameBa = cs_dev->getFileName().toLocal8Bit();
    char* filename = filenameBa.data();


    Veracity::u32 return_results_sizeI;
    Veracity::u32 return_results_countI;
    QByteArray resultBA(1024*1024,  0);


    Veracity::u32 status = m_csConnection->FileQuery(
        false,  // distinct; true - do not dublicate resilts
        true, // false ( channel )
        true, // filename
        true, //return_first_ms
        0, // string_start
        0, // string_length
        filename, // every file
        0, // channel
        false, //use_channel
        0, //limit
        false, //order_descending
        0, //time_start
        resultBA.data(),
        &return_results_sizeI, &return_results_countI);


    if (status!=Veracity::ISFS::STATUS_SUCCESS)
        return false;

    QList<ColdstoreFile> fileList;

    getFiles(fileList, resultBA);

    ColdstoreFile file = fileList.at(fileList.size()-2);
    getFileFromeColdStore(file.name, file.ms);

    return true;
}

void ColdStoreStreamReader::destroy()
{
    if (m_csConnection)
    {
        m_csConnection->Disconnect();
        delete m_csConnection;
    }
}

void ColdStoreStreamReader::getFileFromeColdStore(const QString& filename, quint64 time )
{
    
    QByteArray ba = filename.toLocal8Bit();
    char* cfilename = ba.data();


    Veracity::u32 stream_no;
    Veracity::u64 time_current;
    Veracity::u64 file_size;


    if (m_csConnection->Open(
        cfilename, 
        time, 0, 
        &stream_no, 0, 
        &file_size, 
        0,  &time_current) != Veracity::ISFS::STATUS_SUCCESS)
        return;
    
    char* buf = new char[file_size];
    Veracity::u64 data_remaining;
    Veracity::u64 returned_data_length;
    Veracity::u32 result = m_csConnection->Read(stream_no,
        buf,
        file_size,
        file_size,
        &returned_data_length,
        &data_remaining);

    if (result != Veracity::ISFS::STATUS_SUCCESS)
        return;
        
        
    m_csConnection->Close(stream_no);

    FILE* f = fopen("C:/Users/Sergey/Desktop/tests/test.264", "wb");
    fwrite(buf, returned_data_length, 1, f);
    fclose(f);
    
    

}

