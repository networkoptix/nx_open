#ifndef QN_AVI_PLAYLIST_ARCHIVE_DELEGATE_H
#define QN_AVI_PLAYLIST_ARCHIVE_DELEGATE_H

#include <QtCore/QStringList>
#include <QtCore/QFile>

#include <core/resource/avi/avi_archive_delegate.h>


class QnAVIPlaylistArchiveDelegate : public QnAviArchiveDelegate
{
    Q_OBJECT;

public:
    QnAVIPlaylistArchiveDelegate();
    virtual ~QnAVIPlaylistArchiveDelegate();

    virtual bool open(
        const QnResourcePtr &resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher) override;
    virtual void close() override;
    virtual qint64 seek(qint64 time, bool findIFrame) override;
    virtual qint64 endTime() const override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;

    static QString addDirPath(const QString sourceDir, const QString& postfix);

protected:
    struct CLFileInfo
    {
        QString m_name;
        qint64 m_offsetInMks; // offset in micro seconds from the beginning (duration sum of previous files)
        AVFormatContext* m_formatContext;
        void* opaque;
    };

protected:
    virtual qint64 packetTimestamp(const AVPacket& packet);
    virtual bool findStreams();


    virtual QStringList getPlaylist(const QString& url) = 0;
    virtual bool switchToFile(int newFileIndex);
    virtual AVIOContext* getIOContext();
    virtual qint32 readPacket(quint8* buf, int size);
    
    // seek to specified position. If functionis not implemeted, ffmpeg seek method is used (may be more slow)
    virtual bool directSeekToPosition(qint64 /*pos_mks*/) { return false;}

    virtual void fillAdditionalInfo(CLFileInfo* /*fi*/) {}
    virtual void deleteFileInfo(CLFileInfo* fi);
protected:
    quint8* m_ioBuffer;
    AVIOContext* m_ffmpegIOContext;
    int m_currentFileIndex;
    QVector<CLFileInfo*> m_fileList;   
    bool m_inSeek;
private:
    friend struct CLAVIPlaylistStreamReaderPriv;
    qint32 writePacket(quint8* buf, int size);
    qint64 seek(qint64 offset, qint32 whence);
    qint64 findFileIndexByTime(quint64 mksec);
private:
    QFile m_currentFile;
    qint64 m_totalContentLength;
    QSharedPointer<QnDefaultResourceVideoLayout> m_defaultVideoLayout;
};

#endif // QN_AVI_PLAYLIST_ARCHIVE_DELEGATE_H

