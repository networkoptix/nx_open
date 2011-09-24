#ifndef coldstore_stream_reader_h2053
#define coldstore_stream_reader_h2053

#include "../archive/abstract_archive_stream_reader.h"
#include "coldstore_api/ISFS.h"

struct CSFrameInfo
{
    unsigned int shift;
    unsigned int lenght;
    quint64 time;
};


class ColdStoreStreamReader: public CLAbstractArchiveReader
{
public:
    ColdStoreStreamReader(CLDevice* dev);
    virtual ~ColdStoreStreamReader();


    virtual CLAbstractMediaData* getNextData();
    quint64 currentTime(void) const;
    bool isNegativeSpeedSupported() const;
    void channeljumpTo(quint64,int);
private:
    bool init();
    void destroy();

    bool getFileFromeColdStore(const QString& filename, quint64 time);

private:
    bool m_inited;
    Veracity::ISFS* m_csConnection;
    QByteArray m_fileContent;

    QList<CSFrameInfo> m_frameInfo;
    int m_curr_frame;

};

#endif //coldstore_stream_reader_h2053
