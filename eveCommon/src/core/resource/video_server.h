#ifndef __VIDEO_SERVER_H
#define __VIDEO_SERVER_H

#include <QSharedPointer>
#include "core/resource/resource.h"


//class QnRtspListener;

class QnVideoServer: public QnResource
{
public:
    QnVideoServer();
    virtual ~QnVideoServer();
    virtual QString getUniqueId() const;
    virtual QnAbstractStreamDataProvider* createDataProviderInternal(ConnectionRole role);
#if 0
    void startRTSPListener(const QHostAddress& address = QHostAddress::Any, int port = 50000);
    void stopRTSPListener();
private:
    QnRtspListener* m_rtspListener;
#endif
};

typedef QSharedPointer<QnVideoServer> QnVideoServerPtr;
typedef QList<QnVideoServerPtr> QnVideoServerList;

#endif
