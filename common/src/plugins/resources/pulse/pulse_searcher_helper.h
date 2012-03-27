#ifndef pulse_searcher_helper_h1338
#define pulse_searcher_helper_h1338

#include "core/resourcemanagment/resource_searcher.h"



class QnPlPulseSearcherHelper
{
public:
    struct WSResult
    {
        QString ip;
        QString disc_ip;
        QString mac;
        QString name;
        QString manufacture;
    };

    QnPlPulseSearcherHelper();
    ~QnPlPulseSearcherHelper();

    QList<WSResult> findResources();

private:
    WSResult parseReply(QByteArray& datagram);
};

#endif // onvif_ws_searcher_helper_h1338
