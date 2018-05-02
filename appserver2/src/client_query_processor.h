/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef CLIENT_QUERY_PROCESSOR_H
#define CLIENT_QUERY_PROCESSOR_H

#include <map>

#include <QtCore/QObject>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <nx/utils/concurrent.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/scoped_thread_rollback.h>
#include <nx/network/deprecated/asynchttpclient.h>

#include "ec2_thread_pool.h"
#include "rest/request_params.h"
#include "transaction/binary_transaction_serializer.h"
#include "transaction/json_transaction_serializer.h"
#include "transaction/transaction.h"
#include "nx/fusion/serialization/ubjson.h"
#include <nx/network/http/custom_headers.h>
#include "api/model/audit/auth_session.h"
#include <common/common_module.h>

namespace {
Qn::SerializationFormat serializationFormatFromUrl(const QUrl &url, Qn::SerializationFormat defaultFormat = Qn::UbjsonFormat)
{
    Qn::SerializationFormat format = defaultFormat;
    QString formatString = QUrlQuery(url).queryItemValue(lit("format"));
    if (!formatString.isEmpty())
        format = QnLexical::deserialized(formatString, defaultFormat);
    return format;
}

Qn::SerializationFormat serializationFormatFromUrl(
    const nx::utils::Url& url,
    Qn::SerializationFormat defaultFormat = Qn::UbjsonFormat)
{
    Qn::SerializationFormat format = defaultFormat;
    QString formatString = QUrlQuery(url.toQUrl()).queryItemValue(lit("format"));
    if (!formatString.isEmpty())
        format = QnLexical::deserialized(formatString, defaultFormat);
    return format;
}

} // anonymous namespace

namespace ec2
{
    static const size_t RESPONSE_WAIT_TIMEOUT_MS = 30*1000;
    static const size_t TCP_CONNECT_TIMEOUT_MS = 10*1000;

    class ClientQueryProcessor: public QObject, public QnCommonModuleAware
    {
        Q_OBJECT
    public:

        ClientQueryProcessor(QnCommonModule* commonModule):
            QnCommonModuleAware(commonModule)
        {
        }

        virtual ~ClientQueryProcessor()
        {
            pleaseStopSync();
        }

        void pleaseStopSync(bool checkForLocks = true)
        {
            QnMutexLocker lk(&m_mutex);
            while (!m_runningHttpRequests.empty())
            {
                nx::network::http::AsyncHttpClientPtr httpClient = m_runningHttpRequests.begin()->first;
                lk.unlock();    //must unlock mutex to avoid deadlock with http completion handler
                httpClient->pleaseStopSync(checkForLocks);
                //it is garanteed that no http event handler is running currently and no handler will be called
                lk.relock();
                m_runningHttpRequests.erase(m_runningHttpRequests.begin());
            }
        }

        //!Asynchronously executes update query
        /*!
            \param handler Functor ( ErrorCode )
        */
        template<class InputData, class HandlerType>
            void processUpdateAsync( const nx::utils::Url& ecBaseUrl, ApiCommand::Value cmdCode, InputData input, HandlerType handler )
        {
            nx::utils::Url requestUrl( ecBaseUrl );
            nx::network::http::AsyncHttpClientPtr httpClient = nx::network::http::AsyncHttpClient::create();
            httpClient->setResponseReadTimeoutMs( RESPONSE_WAIT_TIMEOUT_MS );
            httpClient->setSendTimeoutMs( TCP_CONNECT_TIMEOUT_MS );
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }
            addCustomHeaders(httpClient);

            requestUrl.setPath( lit("/ec2/%1").arg(ApiCommand::toString(cmdCode)) );

            QByteArray serializedData;
            Qn::SerializationFormat format = serializationFormatFromUrl(ecBaseUrl);
            if( format == Qn::JsonFormat )
                serializedData = QJson::serialized(input);
            else if( format == Qn::UbjsonFormat )
                serializedData = QnUbjson::serialized(input);
            else
            {
                NX_ASSERT(false);
            }

            connect( httpClient.get(), &nx::network::http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QnMutexLocker lk( &m_mutex );
            httpClient->doPost(
                requestUrl,
                Qn::serializationFormatToHttpContentType(format),
                std::move(serializedData));
            auto func = [this, httpClient, handler](){ processHttpPostResponse( httpClient, handler ); };
            m_runningHttpRequests[httpClient] = std::function<void()>( func );
        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO allow compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( const nx::utils::Url& ecBaseUrl, ApiCommand::Value cmdCode, InputData input, HandlerType handler )
        {
            nx::utils::Url requestUrl( ecBaseUrl );
            nx::network::http::AsyncHttpClientPtr httpClient = nx::network::http::AsyncHttpClient::create();
            httpClient->setResponseReadTimeoutMs( RESPONSE_WAIT_TIMEOUT_MS );
            httpClient->setSendTimeoutMs( TCP_CONNECT_TIMEOUT_MS );
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }
            addCustomHeaders(httpClient);

            requestUrl.setPath( lit("/ec2/%1").arg(ApiCommand::toString(cmdCode)) );
            QUrlQuery query;
            toUrlParams(input, &query);
            Qn::SerializationFormat format = serializationFormatFromUrl(ecBaseUrl);

            query.addQueryItem("format", QnLexical::serialized(format));
            requestUrl.setQuery(query);

            connect( httpClient.get(), &nx::network::http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QnMutexLocker lk( &m_mutex );
            httpClient->doGet( requestUrl );
            auto func = std::bind( std::mem_fn( &ClientQueryProcessor::processHttpGetResponse<OutputData, HandlerType> ), this, httpClient, handler );
            m_runningHttpRequests[httpClient] = std::function<void()>( func );
        }

    public slots:
        void onHttpDone( nx::network::http::AsyncHttpClientPtr httpClient )
        {
            std::function<void()> handler;
            {
                QnMutexLocker lk( &m_mutex );
                auto it = m_runningHttpRequests.find( httpClient );
                NX_ASSERT( it != m_runningHttpRequests.end() );
                handler = std::move(it->second);
                httpClient->pleaseStopSync();
                m_runningHttpRequests.erase( it );
            }

            handler();
        }

    private:
        QnMutex m_mutex;
        std::map<nx::network::http::AsyncHttpClientPtr, std::function<void()> > m_runningHttpRequests;

        template<class OutputData, class HandlerType>
            void processHttpGetResponse( nx::network::http::AsyncHttpClientPtr httpClient, HandlerType handler )
        {
            if( httpClient->failed() || !httpClient->response() )
                return handler( ErrorCode::ioError, OutputData() );
            switch( httpClient->response()->statusLine.statusCode )
            {
                case nx::network::http::StatusCode::ok:
                    break;
                case nx::network::http::StatusCode::unauthorized: {
                    QString authResultStr = nx::network::http::getHeaderValue(httpClient->response()->headers, Qn::AUTH_RESULT_HEADER_NAME);
                    if (!authResultStr.isEmpty()) {
                        Qn::AuthResult authResult = QnLexical::deserialized<Qn::AuthResult>(authResultStr);
                        if (authResult == Qn::Auth_LDAPConnectError)
                            return handler( ErrorCode::ldap_temporary_unauthorized, OutputData() );
                        else if (authResult == Qn::Auth_CloudConnectError)
                            return handler( ErrorCode::cloud_temporary_unauthorized, OutputData() );
                        else if (authResult == Qn::Auth_DisabledUser)
                            return handler(ErrorCode::disabled_user_unauthorized, OutputData());
                    }
                    return handler( ErrorCode::unauthorized, OutputData() );
                }
                case nx::network::http::StatusCode::notImplemented:
                    return handler( ErrorCode::unsupported, OutputData() );
                case nx::network::http::StatusCode::forbidden:
                    return handler(ErrorCode::forbidden, OutputData());
                default:
                    return handler( ErrorCode::serverError, OutputData() );
            }

            const QByteArray& msgBody = httpClient->fetchMessageBodyBuffer();
            OutputData outputData;

            Qn::SerializationFormat format = Qn::serializationFormatFromHttpContentType(httpClient->contentType());
            bool success = false;
            switch( format )
            {
            //case Qn::BnsFormat:
            //    outputData = QnBinary::deserialized<OutputData>(msgBody, OutputData(), &success);
            //    break;
            case Qn::JsonFormat:
                outputData = QJson::deserialized<OutputData>(msgBody, OutputData(), &success);
                break;
            case Qn::UbjsonFormat:
                outputData = QnUbjson::deserialized<OutputData>(msgBody, OutputData(), &success);
                break;
                //case Qn::CsvFormat:
                //    tran = QnCsv::deserialized<OutputData>(msgBody, OutputData(), &success);
                //    break;
                //case Qn::XmlFormat:
                //    tran = QnXml::deserialized<OutputData>(msgBody, OutputData(), &success);
                //    break;
            default:
                NX_ASSERT(false);
            }

            if( !success)
                handler( ErrorCode::badResponse, outputData );
            else
                handler( ErrorCode::ok, outputData );
        }

        template<class HandlerType>
            void processHttpPostResponse( nx::network::http::AsyncHttpClientPtr httpClient, HandlerType handler )
        {
            if( httpClient->failed() || !httpClient->response() )
                return handler( ErrorCode::ioError );
            switch( httpClient->response()->statusLine.statusCode )
            {
                case nx::network::http::StatusCode::ok:
                    return handler( ErrorCode::ok );
                case nx::network::http::StatusCode::unauthorized:
                    return handler( ErrorCode::unauthorized );
                case nx::network::http::StatusCode::forbidden:
                    return handler(ErrorCode::forbidden);
                case nx::network::http::StatusCode::notImplemented:
                    return handler( ErrorCode::unsupported );
                default:
                    return handler( ErrorCode::serverError );
            }
        }
    private:
        void addCustomHeaders(const nx::network::http::AsyncHttpClientPtr& httpClient)
        {
            if (!commonModule()->videowallGuid().isNull())
                httpClient->addAdditionalHeader(Qn::VIDEOWALL_GUID_HEADER_NAME, commonModule()->videowallGuid().toString().toUtf8());
            httpClient->addAdditionalHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, commonModule()->runningInstanceGUID().toByteArray());
            httpClient->addAdditionalHeader(Qn::CUSTOM_CHANGE_REALM_HEADER_NAME, QByteArray()); //< allow to update realm if migration
        }
    };
}

#endif  //CLIENT_QUERY_PROCESSOR_H
