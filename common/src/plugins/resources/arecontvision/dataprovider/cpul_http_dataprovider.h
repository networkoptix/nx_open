#ifndef cpull_httpreader_1119
#define cpull_httpreader_1119

#include <QtNetwork/QAuthenticator>

#include "av_client_pull.h"

//single sensor HTTP reader
class AVClientPullSSHTTPStreamreader : public QnPlAVClinetPullStreamReader
{
public:
    explicit AVClientPullSSHTTPStreamreader(QnResourcePtr res);

    ~AVClientPullSSHTTPStreamreader()
    {
        stop();
    }

protected:

    virtual QnAbstractMediaDataPtr getNextData();

protected:
    unsigned int m_port;
    unsigned int m_timeout;
    QAuthenticator m_auth;

    bool m_panoramic;
    bool m_dualsensor;
    QString m_name;
};

#endif //cpull_httpreader_1119
