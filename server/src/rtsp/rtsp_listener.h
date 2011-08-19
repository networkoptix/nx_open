#include <QObject>
#include <QNetworkInterface>

#include "common/base.h"
#include "common/longrunnable.h"

class QnRtspListener: public QnLongRunnable
{
    Q_OBJECT
public:
    static const int DEFAULT_RTSP_PORT = 554;

    explicit QnRtspListener(const QHostAddress& address = QHostAddress::Any, int port = DEFAULT_RTSP_PORT);
    virtual ~QnRtspListener();
protected:
    virtual void run();
private:
    QN_DECLARE_PRIVATE(QnRtspListener);
};
