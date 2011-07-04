#ifndef __AVI_DVD_STREAM_READER_H
#define __AVI_DVD_STREAM_READER_H

#include <QStringList>
#include <QFile>

#include "avi_playlist_strem_reader.h"

struct dvd_reader_t;

class CLAVIDvdStreamReader : public CLAVIPlaylistStreamReader
{
public:
	CLAVIDvdStreamReader(CLDevice* dev);
	virtual ~CLAVIDvdStreamReader();
    void setChapterNum(int chupter);
protected:
    virtual void destroy();
    virtual QStringList getPlaylist();
    virtual ByteIOContext* getIOContext();
    virtual bool switchToFile(int newFileIndex);
    virtual qint32 readPacket(quint8* buf, int size);
    virtual void fillAdditionalInfo(CLFileInfo* fi);
private:
    friend class CLAVIDvdStreamReaderPriv;
    qint64 seek(qint64 offset, qint32 whence);
    qint32 writePacket(quint8* buf, int size);
private:
    int m_chapter;
    dvd_reader_t* m_dvdReader;

    quint8* m_tmpBuffer;
    quint8* m_dvdReadBuffer;
    quint8* m_dvdAlignedBuffer;
    int m_tmpBufferSize;
    //qint64 m_currentFileSize;
    qint64 m_currentPosition;
};

#endif __AVI_DVD_STREAM_READER_H
