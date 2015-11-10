#ifndef pulse_searcher_helper_h1338
#define pulse_searcher_helper_h1338

#ifdef ENABLE_PULSE_CAMERA

#include "core/resource_management/resource_searcher.h"



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
    WSResult parseReply(const QByteArray& datagram);
};

#endif // #ifdef ENABLE_PULSE_CAMERA
#endif // onvif_ws_searcher_helper_h1338
