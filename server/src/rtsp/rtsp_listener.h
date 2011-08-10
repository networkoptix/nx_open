#include <QObject>
#include <QNetworkInterface>

#include "common/utils/common/base.h"

class QnRtspListener: public QObject
{
    Q_OBJECT
public:
    static const int DEFAULT_RTSP_PORT = 554;

    explicit QnRtspListener(const QHostAddress& address = QHostAddress::Any, int port = DEFAULT_RTSP_PORT);
    virtual ~QnRtspListener();
private slots:
    void onNewConnection();
private:
    QN_DECLARE_PRIVATE(QnRtspListener);
};
