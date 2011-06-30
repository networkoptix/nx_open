#ifndef __AVI_PLAYLIST_STREAM_READER_H
#define __AVI_PLAYLIST_STREAM_READER_H

#include <QStringList>
#include <QFile>

#include "avi_strem_reader.h"

class CLAVIPlaylistStreamReader : public CLAVIStreamReader
{
public:
	CLAVIPlaylistStreamReader(CLDevice* dev);
	virtual ~CLAVIPlaylistStreamReader();

    static QString addDirPath(const QString sourceDir, const QString& postfix);

protected:
    struct CLFileInfo
    {
        QString m_name;
        qint64 m_offsetInMks; // offset in micro seconds from the beginning (duration sum of previous files)
        AVFormatContext* m_formatContext;
        qint64 m_durationHint; // override play item duration from hint instead ffmpeg autodetection. Ffmpeg fails (invalid duration) for some VOB files
        void* opaque;
    };

protected:
    virtual void channeljumpTo(quint64 mksec, int /*channel*/);
    virtual qint64 packetTimestamp(AVStream* stream, const AVPacket& packet);
    virtual void destroy();
    virtual AVFormatContext* getFormatContext();

    virtual QStringList getPlaylist() = 0;
    virtual bool switchToFile(int newFileIndex);
    virtual ByteIOContext* getIOContext();
    virtual qint32 readPacket(quint8* buf, int size);
protected:
    quint8* m_ioBuffer;
    ByteIOContext* m_ffmpegIOContext;
    int m_currentFileIndex;
    QVector<CLFileInfo*> m_fileList;   
    bool m_initialized;             // file list has been built
private:
    friend class CLAVIPlaylistStreamReaderPriv;
    qint32 writePacket(quint8* buf, int size);
    qint64 seek(qint64 offset, qint32 whence);
    qint64 findFileIndexByTime(quint64 mksec);
private:
    QFile m_currentFile;            
    bool m_inSeek;
};

#endif __AVI_PLAYLIST_STREAM_READER_H
