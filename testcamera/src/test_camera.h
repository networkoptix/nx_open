#ifndef __TEST_CAMERA_H__
#define __TEST_CAMERA_H__

#include <QTime>
#include <QStringList>
#include <QMap>
#include <QFile>
#include "utils/network/socket.h"
#include "core/datapacket/mediadatapacket.h"

class QnTestCamera
{
public:

    QnTestCamera(quint32 num);

    QByteArray getMac() const;
    void setFileList(const QStringList& files);
    void setFps(double fps);
    void setOfflineFreq(double offlineFreq);

    void startStreaming(TCPSocket* socket);

    bool isEnabled();
private:
    bool doStreamingFile(QList<QnCompressedVideoDataPtr> data, TCPSocket* socket);
    void makeOfflineFlood();
private:
    quint32 m_num;
    QByteArray m_mac;
    QStringList m_files;
    int m_prefixLen;
    int m_offlineFreq;
    double m_fps;
    QnMediaContextPtr m_context;
    bool m_isEnabled;
    QTime m_offlineTimer;
    QTime m_checkTimer;
    int m_offlineDuration;
};

class QnFileCache
{
public:
    static QnFileCache* instance();

    QList<QnCompressedVideoDataPtr> getMediaData(const QString& fileName);
private:
    typedef QMap<QString, QList<QnCompressedVideoDataPtr> > MediaCache;
    MediaCache m_cache;
    QMutex m_mutex;
};

#endif // __TEST_CAMERA_H__
