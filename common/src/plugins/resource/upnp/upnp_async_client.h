#ifndef UPNP_ASYNC_CLIENT_H
#define UPNP_ASYNC_CLIENT_H

#include "utils/network/http/asynchttpclient.h"

class UpnpAsyncClient;
typedef std::shared_ptr<UpnpAsyncClient> UpnpAsyncClientPtr;

//! UPnP SOAP based client
class UpnpAsyncClient
    : public nx_http::AsyncHttpClient
{
public:
    //! Simple SOAP call
    struct Message
    {
        QString action;
        QString service;
        std::map<QString, QString> params;
    };

    typedef std::function< void( const Message& )> Callback;

    //!Creates request by @param message and calls @fn doPost
    bool doUpnp( const QUrl& url, const Message& message, const Callback& callback );
};

#endif // UPNP_ASYNC_CLIENT_H
