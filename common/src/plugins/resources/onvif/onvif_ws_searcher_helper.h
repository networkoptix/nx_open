#ifndef onvif_ws_searcher_helper_h1338
#define onvif_ws_searcher_helper_h1338

#include "core/resourcemanagment/resource_searcher.h"



class QnPlOnvifWsSearcherHelper
{
public:
    struct WSResult
    {
        QString ip;
        QString mac;
        QString name;
        QString manufacture;
    };

    QnPlOnvifWsSearcherHelper();
    ~QnPlOnvifWsSearcherHelper();

    QList<WSResult> findResources();

private:

};

#endif // onvif_ws_searcher_helper_h1338
