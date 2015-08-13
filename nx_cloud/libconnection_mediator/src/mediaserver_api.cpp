#include "mediaserver_api.h"

#include <utils/network/http/httpclient.h>
#include <utils/common/log.h>

#include <QJsonDocument>

namespace nx {
namespace hpm {

MediaserverApiIf::~MediaserverApiIf()
{
}

bool MediaserverApi::ping( const SocketAddress& address, const QnUuid& expectedId )
{
    QUrl url( address.address.toString() );
    url.setPort( address.port );
    url.setPath( lit("/api/ping") );

    nx_http::HttpClient client;
    if( !client.doGet( url ) )
    {
        NX_LOG( lit("%1 Url %2 is unaccesible")
                .arg( Q_FUNC_INFO ).arg( url.toString() ), cl_logDEBUG1 )
        return false;
    }

    const auto buffer = client.fetchMessageBodyBuffer();
    const QVariantMap responce = QJsonDocument::fromJson(buffer).toVariant().toMap();
    if( responce.isEmpty() ) {
        NX_LOG( lit("%1 Url %2 response is invalid JSON: %3")
                .arg( Q_FUNC_INFO ).arg( url.toString() )
                .arg( QString::fromUtf8( buffer ) ), cl_logDEBUG1 );
        return false;
    }

    const auto reply = responce.value( lit("reply") ).toMap();
    if( reply.isEmpty() ) {
        NX_LOG( lit("%1 Url %2 response does not contain valid field reply: ")
                .arg( Q_FUNC_INFO ).arg( url.toString() ), cl_logDEBUG1 );
        return false;
    }

    const auto moduleGuid = reply.value( lit("moduleGuid") ).toString();
    if( moduleGuid.isEmpty() ) {
        NX_LOG( lit("%1 Url %2 reply does not contain valid field moduleGuid")
                .arg( Q_FUNC_INFO ).arg( url.toString() ), cl_logDEBUG1 );
        return false;
    }

    const QnUuid urlId( moduleGuid );
    if( urlId.isNull() ) {
        NX_LOG( lit("%1 Url %2 returns invalid moduleGuid: %3")
                .arg( Q_FUNC_INFO ).arg( url.toString() )
                .arg( moduleGuid ), cl_logDEBUG1 );
        return false;
    }

    if( urlId != expectedId) {
        NX_LOG( lit("%1 Url %2 moduleGuid %3 doesnt match expectedId %4")
                .arg( Q_FUNC_INFO ).arg( url.toString() )
                .arg( urlId.toString() ).arg( expectedId.toString() ), cl_logDEBUG1 );
        return false;
    }

    return true;
}

} // namespace hpm
} // namespace nx
