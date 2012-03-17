#include "onvif_ws_searcher.h"
#include "onvif_ws_searcher_helper.h"


QnPlOnvifWsSearcher::QnPlOnvifWsSearcher()
{

}

QnPlOnvifWsSearcher& QnPlOnvifWsSearcher::instance()
{
    static QnPlOnvifWsSearcher inst;
    return inst;
}

QnResourceList QnPlOnvifWsSearcher::findResources()
{
    QnResourceList result;

    QnPlOnvifWsSearcherHelper helper;
    QList<QnPlOnvifWsSearcherHelper::WSResult> onnvifResponses = helper.findResources();

    return result;
}

QnResourcePtr QnPlOnvifWsSearcher::createResource(QnId resourceTypeId, const QnResourceParameters &parameters)
{
    return QnResourcePtr(0);
}

QnResourcePtr QnPlOnvifWsSearcher::checkHostAddr(QHostAddress addr)
{
    return QnResourcePtr(0);
}

QString QnPlOnvifWsSearcher::manufacture() const
{
    return "";
}
