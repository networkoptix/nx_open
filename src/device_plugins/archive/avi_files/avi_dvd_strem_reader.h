#ifndef __AVI_DVD_STREAM_READER_H
#define __AVI_DVD_STREAM_READER_H

#include <QStringList>
#include <QFile>

#include "avi_strem_reader.h"

class CLAVIDvdStreamReader : public CLAVIStreamReader
{
    struct CLFileInfo
    {
        QString m_name;
        qint64 m_offsetInMks; // offset in micro seconds from the beginning (duration sum of previous files)
        AVFormatContext* m_formatContext;
        AVIOContext* m_orig_pb;
    };
public:
	CLAVIDvdStreamReader(CLDevice* dev);
	virtual ~CLAVIDvdStreamReader();
    void setChapterNum(int chupter);
protected:
    ByteIOContext* getIOContext();
    virtual void channeljumpTo(quint64 mksec, int /*channel*/);
    virtual qint64 packetTimestamp(AVStream* stream, const AVPacket& packet);
    virtual void destroy();
    virtual AVFormatContext* getFormatContext();
protected:
    quint8* m_ioBuffer;
    ByteIOContext* m_ffmpegIOContext;
private:
    friend class CLAVIDvdStreamReaderPriv;
    qint32 readPacket(quint8* buf, int size);
    qint32 writePacket(quint8* buf, int size);
    qint64 seek(qint64 offset, qint32 whence);
    bool openNextFile();
    qint64 setSingleFileMode(quint64 mksec);
    qint64 seekToFile(qint64 offset);
private:
    QFile m_currentFile;            
    int m_currentFileIndex;
    int m_chapter;                  // dvd chapter number, -1 by default: play all chapters
    bool m_initialized;             // file list has been built
    QList<CLFileInfo*> m_fileList;   
    bool m_inSeek;
};

#endif __AVI_DVD_STREAM_READER_H
