#ifndef __TEST_CAMERA_H__
#define __TEST_CAMERA_H__

#include <QMap>
#include <QFile>
#include "utils/network/socket.h"

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
    bool doStreamingFile(const QByteArray data, TCPSocket* socket);
    
    bool streamingH264(const QByteArray data, TCPSocket* socket);
    bool streamingMJPEG(const QByteArray data, TCPSocket* socket);

    const quint8* findH264FrameEnd(const quint8* curPtr, const quint8* end, bool* isKeyData);
private:
    quint32 m_num;
    QByteArray m_mac;
    QStringList m_files;
    int m_prefixLen;
    int m_offlineFreq;
    double m_fps;
};

#endif // __TEST_CAMERA_H__
