/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include <chrono>
#include <iostream>
#include <iomanip>

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/network/http/httpclient.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>


struct TestAddressDescriptor
{
    const char* address;
    const char* description;
};

static TestAddressDescriptor kCloudAddressList[] = {
    {"0d8dcea3-057a-4f44-81a4-d70e9127ab14", "la_office"}
};

constexpr const int kSendTimeoutMs = 11*1000;

constexpr const char* kLogFileUploadUrl = "http://hdw.mx/cloud_connect_log_file";

int main(int /*argc*/, char* /*argv*/[])
{
    using namespace std::chrono;

    QDir().remove("cloud_connect_auto_test.log");

    //initializing log
    QnLog::instance()->create(
        QLatin1String("./cloud_connect_auto_test"),
        50 * 1000 * 1000,
        2,
        cl_logDEBUG2);
    QnLog::instance()->setLogLevel(cl_logDEBUG2);

    nx::network::SocketGlobals::InitGuard socketInitializationGuard;
    nx::network::SocketGlobals::mediatorConnector().enable(true);

    bool succeeded = true;
    for (const TestAddressDescriptor& addressDescriptor: kCloudAddressList)
    {
        const auto startConnectingTimepoint = std::chrono::steady_clock::now();
        NX_LOG(lm("Connecting to %1 (%2)").arg(addressDescriptor.description)
            .arg(addressDescriptor.address), cl_logDEBUG1);

        std::cout<<"Connecting to "<<addressDescriptor.description<<" >>   ";
        QUrl url(lit("http://%1/api/ping").arg(addressDescriptor.address));
        nx_http::HttpClient httpClient;
        httpClient.setSendTimeoutMs(kSendTimeoutMs);
        httpClient.setResponseReadTimeoutMs(kSendTimeoutMs);
        httpClient.setMessageBodyReadTimeoutMs(kSendTimeoutMs);
        const bool result = httpClient.doGet(url);
        auto connectDuration = std::chrono::steady_clock::now() - startConnectingTimepoint;
        if (!result)
        {
            succeeded = false;
            NX_LOG(lm("FAILURE (%1ms). %2")
                .arg(duration_cast<milliseconds>(connectDuration).count())
                .arg(SystemError::toString(httpClient.lastSysErrorCode())),
                cl_logDEBUG1);
            std::cout << "FAILURE (" 
                << duration_cast<milliseconds>(connectDuration).count() <<" ms). "
                << SystemError::toString(httpClient.lastSysErrorCode()).toStdString()
                << std::endl;
            continue;
        }

        nx_http::BufferType msgBodyBuf;
        while (!httpClient.eof())
            msgBodyBuf += httpClient.fetchMessageBodyBuffer();

        connectDuration = std::chrono::steady_clock::now() - startConnectingTimepoint;

        if (nx_http::StatusCode::isSuccessCode(httpClient.response()->statusLine.statusCode))
        {
            NX_LOG(lm("SUCCESS (%1ms)")
                .arg(duration_cast<milliseconds>(connectDuration).count()),
                cl_logDEBUG1);
            std::cout << "SUCCESS ("
                << duration_cast<milliseconds>(connectDuration).count()
                << "ms)" << std::endl;
        }
        else
        {
            succeeded = false;
            NX_LOG(lm("PARTIAL SUCCESS (%1ms). %2")
                .arg(duration_cast<milliseconds>(connectDuration).count())
                .arg(httpClient.response()->statusLine.statusCode), cl_logDEBUG1);
            std::cout << "PARTIAL SUCCESS ("
                << duration_cast<milliseconds>(connectDuration).count() <<"ms). "
                << httpClient.response()->statusLine.statusCode
                << std::endl;
        }
    }

    if (!succeeded)
    {
        //on failure uploading log to some server
        std::cout<<"Uploading log file...    ";
        QFile logFile("cloud_connect_auto_test.log");
        if (!logFile.open(QFile::ReadOnly))
        {
            std::cout<<"Could not open log"<<std::endl;
            return 1;
        }
        const auto logFileContents = logFile.readAll();
        nx_http::HttpClient httpClient;
        if (!httpClient.doPost(QUrl(kLogFileUploadUrl), "application/text", logFileContents))
        {
            std::cout << "Could not upload log" << std::endl;
            return 2;
        }

        std::cout << "done" << std::endl;
    }

    return 0;
}
