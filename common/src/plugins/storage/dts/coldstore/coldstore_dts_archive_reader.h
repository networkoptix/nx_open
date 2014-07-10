#ifndef coldstore_stream_reader_h1059
#define coldstore_stream_reader_h1059

#ifdef ENABLE_COLDSTORE

#include "plugins/resource/archive/abstract_archive_delegate.h"
#include "../../coldstore/coldstore_api/ISFS.h"


class ColdStoreDevice;

struct CSFrameInfo
{
    unsigned int shift;
    unsigned int lenght;
    quint64 time;
    bool i_frame;
};

struct ColdstoreFile
{
    QString name;
    quint64 mks;
    quint64 original_ms;
    int channel;
    float fps;
};

struct CSEvent
{
    unsigned int shift;
    unsigned int len;
    quint64 time;
    float fps;
};

class QnColdStoreDelegate : public QnAbstractArchiveDelegate
{
public:
    QnColdStoreDelegate(QHostAddress csAddr);
    virtual ~QnColdStoreDelegate();

    virtual bool open(const QnResourcePtr &resource);
    virtual void close();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnResourceAudioLayoutPtr getAudioLayout() override;

private:    

    bool fileListQuery(int channel, QList<ColdstoreFile>& lst);

    

    bool openCSFile(int f_index, int event_index);
    bool openEventFile(int f_index);

    int find_best_file(quint64 time);
    int find_best_event(quint64 time);

private:

    QHostAddress m_csAddr;

    bool m_opend;
	bool m_loggedError;

    
    Veracity::ISFS* m_csConnection;
    QByteArray m_fileContent;

    QList<ColdstoreFile> m_datafileList;
    QList<ColdstoreFile> m_eventfileList;

    QList<CSFrameInfo> m_frameInfo;

    QList<CSEvent> m_eventInfo;

    int m_current_event;
    int m_curr_frame;
    int m_current_file;

    Veracity::u32 m_stream_no;
    int  m_openedFile;
    Veracity::u64 m_file_size;
    qint64 m_startTime;
    qint64 m_endTime;

    QnResourcePtr m_resource;

};


#endif // ENABLE_COLDSTORE

#endif //coldstore_stream_reader_h1059
