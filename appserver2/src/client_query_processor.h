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
#include <utils/common/concurrent.h>
#include <utils/common/model_functions.h>
#include <utils/common/scoped_thread_rollback.h>
#include <nx/network/http/asynchttpclient.h>

#include "ec2_thread_pool.h"
#include "rest/request_params.h"
#include "transaction/binary_transaction_serializer.h"
#include "transaction/json_transaction_serializer.h"
#include "transaction/transaction.h"
#include "utils/serialization/ubjson.h"
#include "http/custom_headers.h"
#include "api/model/audit/auth_session.h"

// TODO mike: REMOVE
#define OUTPUT_PREFIX "[mike] client_query_processor: "
#include <nx/utils/debug_utils.h>

namespace {
    Qn::SerializationFormat serializationFormatFromUrl(const QUrl &url, Qn::SerializationFormat defaultFormat = Qn::UbjsonFormat) {
        Qn::SerializationFormat format = defaultFormat;
        QString formatString = QUrlQuery(url).queryItemValue(lit("format"));
        if (!formatString.isEmpty())
            format = QnLexical::deserialized(formatString, defaultFormat);
        return format;
    }
} // anonymous namespace

namespace ec2
{
    static const size_t RESPONSE_WAIT_TIMEOUT_MS = 30*1000;
    static const size_t TCP_CONNECT_TIMEOUT_MS = 10*1000;

    class ClientQueryProcessor
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~ClientQueryProcessor()
        {
            QnMutexLocker lk( &m_mutex );
            while( !m_runningHttpRequests.empty() )
            {
                nx_http::AsyncHttpClientPtr httpClient = m_runningHttpRequests.begin()->first;
                lk.unlock();    //must unlock mutex to avoid deadlock with http completion handler
                httpClient->terminate();
                //it is garanteed that no http event handler is running currently and no handler will be called
                lk.relock();
                m_runningHttpRequests.erase( m_runningHttpRequests.begin() );
            }
        }

        //!Asynchronously executes update query
        /*!
            \param handler Functor ( ErrorCode )
        */
        template<class QueryDataType, class HandlerType>
            void processUpdateAsync( const QUrl& ecBaseUrl, const QnTransaction<QueryDataType>& tran, HandlerType handler )
        {
            QUrl requestUrl( ecBaseUrl );
            nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
            httpClient->setResponseReadTimeoutMs( RESPONSE_WAIT_TIMEOUT_MS );
            httpClient->setSendTimeoutMs( TCP_CONNECT_TIMEOUT_MS );
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }
            httpClient->addAdditionalHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, qnCommon->runningInstanceGUID().toByteArray());

            requestUrl.setPath( lit("/ec2/%1").arg(ApiCommand::toString(tran.command)) );

            QByteArray tranBuffer;
            Qn::SerializationFormat format = serializationFormatFromUrl(ecBaseUrl);
            if( format == Qn::JsonFormat )
                tranBuffer = QJson::serialized(tran);
            //else if( format == Qn::BnsFormat )
            //    tranBuffer = QnBinary::serialized(tran);
            else if( format == Qn::UbjsonFormat )
                tranBuffer = QnUbjson::serialized(tran);
            //else if( format == Qn::CsvFormat )
            //    tranBuffer = QnCsv::serialized(tran);
            //else if( format == Qn::XmlFormat )
            //    tranBuffer = QnXml::serialized(tran, lit("reply"));
            else
            {
                NX_ASSERT(false);
            }

            connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QnMutexLocker lk( &m_mutex );
            httpClient->doPost(
                requestUrl,
                Qn::serializationFormatToHttpContentType(format),
                std::move(tranBuffer));
            auto func = [this, httpClient, handler](){ processHttpPostResponse( httpClient, handler ); };
            m_runningHttpRequests[httpClient] = std::function<void()>( func );
        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO allow compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( const QUrl& ecBaseUrl, ApiCommand::Value cmdCode, InputData input, HandlerType handler )
        {
PRINT << "processQueryAsync(): Input url: " <<  ecBaseUrl;
            QUrl requestUrl( ecBaseUrl );
            nx_http::AsyncHttpClientPtr httpClient = nx_http::AsyncHttpClient::create();
            httpClient->setResponseReadTimeoutMs( RESPONSE_WAIT_TIMEOUT_MS );
            httpClient->setSendTimeoutMs( TCP_CONNECT_TIMEOUT_MS );
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }

            //TODO #ak videowall looks strange here
            if (!QnAppServerConnectionFactory::videowallGuid().isNull())
                httpClient->addAdditionalHeader(Qn::VIDEOWALL_GUID_HEADER_NAME, QnAppServerConnectionFactory::videowallGuid().toString().toUtf8());
            httpClient->addAdditionalHeader(Qn::EC2_RUNTIME_GUID_HEADER_NAME, qnCommon->runningInstanceGUID().toByteArray());

            requestUrl.setPath( lit("/ec2/%1").arg(ApiCommand::toString(cmdCode)) );
            QUrlQuery query;
            toUrlParams(input, &query);
            Qn::SerializationFormat format = serializationFormatFromUrl(ecBaseUrl);

            query.addQueryItem("format", QnLexical::serialized(format));
            requestUrl.setQuery(query);
PRINT << "processQueryAsync(): Output url: " <<  requestUrl;
            QString effectiveUserName = QUrlQuery(ecBaseUrl).queryItemValue(lit("effectiveUserName"));
            if (!effectiveUserName.isEmpty())
            {
                httpClient->setEffectiveUserName(effectiveUserName);
PRINT << "processQueryAsync(): effectiveUserName: " << effectiveUserName;
            }

            connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QnMutexLocker lk( &m_mutex );
            httpClient->doGet( requestUrl );
            auto func = std::bind( std::mem_fn( &ClientQueryProcessor::processHttpGetResponse<OutputData, HandlerType> ), this, httpClient, handler );
            m_runningHttpRequests[httpClient] = std::function<void()>( func );
        }

    public slots:
        void onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
        {
            std::function<void()> handler;
            {
                QnMutexLocker lk( &m_mutex );
                auto it = m_runningHttpRequests.find( httpClient );
                NX_ASSERT( it != m_runningHttpRequests.end() );
                handler = std::move(it->second);
                httpClient->terminate();
                m_runningHttpRequests.erase( it );
            }

            handler();
        }

    private:
        QnMutex m_mutex;
        std::map<nx_http::AsyncHttpClientPtr, std::function<void()> > m_runningHttpRequests;

        template<class OutputData, class HandlerType>
            void processHttpGetResponse( nx_http::AsyncHttpClientPtr httpClient, HandlerType handler )
        {
            if( httpClient->failed() || !httpClient->response() )
                return handler( ErrorCode::ioError, OutputData() );
            switch( httpClient->response()->statusLine.statusCode )
            {
                case nx_http::StatusCode::ok:
                    break;
                case nx_http::StatusCode::unauthorized: {
                    QString authResultStr = nx_http::getHeaderValue(httpClient->response()->headers, Qn::AUTH_RESULT_HEADER_NAME);
                    if (!authResultStr.isEmpty()) {
                        Qn::AuthResult authResult = QnLexical::deserialized<Qn::AuthResult>(authResultStr);
                        if (authResult == Qn::Auth_ConnectError)
                            return handler( ErrorCode::temporary_unauthorized, OutputData() );
                    }
                    return handler( ErrorCode::unauthorized, OutputData() );
                }
                case nx_http::StatusCode::notImplemented:
                    return handler( ErrorCode::unsupported, OutputData() );
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
            void processHttpPostResponse( nx_http::AsyncHttpClientPtr httpClient, HandlerType handler )
        {
            if( httpClient->failed() || !httpClient->response() )
                return handler( ErrorCode::ioError );
            switch( httpClient->response()->statusLine.statusCode )
            {
                case nx_http::StatusCode::ok:
                    return handler( ErrorCode::ok );
                case nx_http::StatusCode::unauthorized:
                    return handler( ErrorCode::unauthorized );
                case nx_http::StatusCode::notImplemented:
                    return handler( ErrorCode::unsupported );
                default:
                    return handler( ErrorCode::serverError );
            }
        }
    };
}

#endif  //CLIENT_QUERY_PROCESSOR_H
