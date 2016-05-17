
#include <iostream>
#include <iomanip>

#include <nx/network/http/httpclient.h>
#include <nx/network/socket_global.h>


struct TestAddressDescriptor
{
    const char* address;
    const char* description;
};

static TestAddressDescriptor kCloudAddressList[] = {
    {"0d8dcea3-057a-4f44-81a4-d70e9127ab14", "la_office"}
};

constexpr const int kSendTimeoutMs = 11*1000;

int main(int /*argc*/, char* /*argv*/[])
{
    nx::network::SocketGlobals::InitGuard socketInitializationGuard;
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    //TODO #ak initializing log

    for (const TestAddressDescriptor& addressDescriptor: kCloudAddressList)
    {
        std::cout<<"Connecting to "<<addressDescriptor.description<<" >>   ";
        QUrl url(lit("http://%1/api/ping").arg(addressDescriptor.address));
        nx_http::HttpClient httpClient;
        httpClient.setSendTimeoutMs(kSendTimeoutMs);
        httpClient.setResponseReadTimeoutMs(kSendTimeoutMs);
        httpClient.setMessageBodyReadTimeoutMs(kSendTimeoutMs);
        if (!httpClient.doGet(url))
        {
            std::cout<<"FAILURE ("<<SystemError::toString(httpClient.lastSysErrorCode()).toStdString()<<std::endl;
            continue;
        }

        nx_http::BufferType msgBodyBuf;
        while (!httpClient.eof())
            msgBodyBuf += httpClient.fetchMessageBodyBuffer();

        if (nx_http::StatusCode::isSuccessCode(httpClient.response()->statusLine.statusCode))
            std::cout << "SUCCESS" << std::endl;
        else
            std::cout << "PARTIAL SUCCESS ("<< httpClient.response()->statusLine.statusCode <<")"<< std::endl;
    }

    //TODO #ak on failure uploading log to some server

    return 0;
}
