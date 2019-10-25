#ifndef __TEST_CAMERA_H__
#define __TEST_CAMERA_H__

#include <QTime>
#include <QStringList>
#include <QMap>
#include <QFile>
#include <QtCore/QMutex>
#include <nx/network/socket.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/streaming/video_data_packet.h>


class QnCommonModule;

class QnTestCamera
{
public:
    QnTestCamera(quint32 num, bool includePts);

    QByteArray getMac() const;

    void setPrimaryFileList(const QStringList& files);
    void setSecondaryFileList(const QStringList& files);

    void setOfflineFreq(double offlineFreq);

    void startStreaming(
        nx::network::AbstractStreamSocket* socket,
        bool isSecondary,
        int fps,
        QnCommonModule* commonModule);

    bool isEnabled();

private:
    bool doStreamingFile(
        QList<QnCompressedVideoDataPtr> data,
        nx::network::AbstractStreamSocket* socket,
        int fps,
        bool isSecondary);

    void makeOfflineFlood();
    int sendAll(nx::network::AbstractStreamSocket* socket, const void* data, int size);

private:
    quint32 m_num;
    const bool m_includePts;
    QByteArray m_mac;
    QStringList m_primaryFiles;
    QStringList m_secondaryFiles;
    int m_prefixLen;
    int m_offlineFreq;
    QnConstMediaContextPtr m_context;
    bool m_isEnabled;
    QTime m_offlineTimer;
    QTime m_checkTimer;
    int m_offlineDuration;
    std::unique_ptr<QFile> m_logFramesFile;
};

class QnFileCache
{
public:
    static QnFileCache* instance();

    QList<QnCompressedVideoDataPtr> getMediaData(const QString& fileName, QnCommonModule* commonModule);
private:
    typedef QMap<QString, QList<QnCompressedVideoDataPtr> > MediaCache;
    MediaCache m_cache;
    QMutex m_mutex;
};

#endif // __TEST_CAMERA_H__
