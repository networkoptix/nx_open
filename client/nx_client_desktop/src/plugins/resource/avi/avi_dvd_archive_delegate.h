#ifndef QN_AVI_DVD_ARCHIVE_DELEGATE_H
#define QN_AVI_DVD_ARCHIVE_DELEGATE_H

#include <QtCore/QStringList>
#include <QtCore/QFile>
#include "avi_playlist_archive_delegate.h"
#include "utils/media/dvd_reader/ifo_types.h"


struct dvd_reader_t;

class QnAVIDvdArchiveDelegate : public QnAVIPlaylistArchiveDelegate
{
    Q_OBJECT;

public:
    QnAVIDvdArchiveDelegate();
    virtual ~QnAVIDvdArchiveDelegate();
    void setChapterNum(int chupter);
    static QStringList getTitleList(const QString& url);

    virtual void close();
protected:
    virtual AVIOContext* getIOContext();
    virtual bool switchToFile(int newFileIndex);
    virtual qint32 readPacket(quint8* buf, int size);
    virtual void fillAdditionalInfo(CLFileInfo* fi);
    virtual bool directSeekToPosition(qint64 pos_mks);
    virtual QStringList getPlaylist(const QString& url);
private:
    friend struct CLAVIDvdStreamReaderPriv;
    qint64 seek(qint64 offset, qint32 whence);
    qint32 writePacket(quint8* buf, int size);
    qint64 packetTimestamp(AVStream* stream, const AVPacket& packet);
    qint64 findFirstDts(quint8* buffer, int bufSize);
    virtual void deleteFileInfo(CLFileInfo* fi);
private:
    int m_chapter;
    dvd_reader_t* m_dvdReader;

    quint8* m_tmpBuffer;
    quint8* m_dvdReadBuffer;
    quint8* m_dvdAlignedBuffer;
    int m_tmpBufferSize;
    //qint64 m_currentFileSize;
    qint64 m_currentPosition;
    ifo_handle_t* m_mainIfo;
};

#endif // QN_AVI_DVD_ARCHIVE_DELEGATE_H
