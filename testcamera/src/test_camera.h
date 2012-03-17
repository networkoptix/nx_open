#ifndef __TEST_CAMERA_H__
#define __TEST_CAMERA_H__

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
private:
    bool doStreamingFile(QList<QnCompressedVideoDataPtr> data, TCPSocket* socket);
    
private:
    quint32 m_num;
    QByteArray m_mac;
    QStringList m_files;
    int m_prefixLen;
    int m_offlineFreq;
    double m_fps;
    QnMediaContextPtr m_context;
};

#endif // __TEST_CAMERA_H__
