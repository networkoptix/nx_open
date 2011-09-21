#ifndef coldstore_stream_reader_h2053
#define coldstore_stream_reader_h2053

#include "../archive/abstract_archive_stream_reader.h"
#include "coldstore_api/ISFS.h"



class ColdStoreStreamReader: public CLAbstractArchiveReader
{
public:
    ColdStoreStreamReader(CLDevice* dev);
    virtual ~ColdStoreStreamReader();


    virtual CLAbstractMediaData* getNextData();
    quint64 currentTime(void) const;
    bool isSpeedSupported(double) const;
    void channeljumpTo(quint64,int);
private:
    bool init();
    void destroy();

    void getFileFromeColdStore(const QString& filename, quint64 time);

private:
    bool m_inited;
    Veracity::ISFS* m_csConnection;

};

#endif //coldstore_stream_reader_h2053