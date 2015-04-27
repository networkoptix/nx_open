/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef CLIENT_QUERY_PROCESSOR_H
#define CLIENT_QUERY_PROCESSOR_H

#include <map>

#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QUrlQuery>

#include <api/app_server_connection.h>
#include <utils/common/concurrent.h>
#include <utils/common/model_functions.h>
#include <utils/common/scoped_thread_rollback.h>
#include <utils/network/http/asynchttpclient.h>

#include "ec2_thread_pool.h"
#include "rest/request_params.h"
#include "transaction/binary_transaction_serializer.h"
#include "transaction/json_transaction_serializer.h"
#include "transaction/transaction.h"
#include "utils/serialization/ubjson.h"


namespace ec2
{
    static const size_t RESPONSE_WAIT_TIMEOUT_MS = 30*1000;

    class ClientQueryProcessor
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~ClientQueryProcessor()
        {
            QMutexLocker lk( &m_mutex );
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
            nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
            httpClient->setResponseReadTimeoutMs( RESPONSE_WAIT_TIMEOUT_MS );
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }

            requestUrl.setPath( lit("/ec2/%1").arg(ApiCommand::toString(tran.command)) );

            QByteArray tranBuffer;
            Qn::SerializationFormat format = Qn::UbjsonFormat; /*Qn::JsonFormat*/;
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
                assert(false);

            connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QMutexLocker lk( &m_mutex );
            if( !httpClient->doPost(requestUrl, Qn::serializationFormatToHttpContentType(format), tranBuffer) )
            {
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, ErrorCode::failure ) );
                return;
            }
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
            QUrl requestUrl( ecBaseUrl );
            nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
            httpClient->setResponseReadTimeoutMs( RESPONSE_WAIT_TIMEOUT_MS );
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }

            //TODO #ak videowall looks strange here
            if (!QnAppServerConnectionFactory::videowallGuid().isNull())
                httpClient->addRequestHeader("X-NetworkOptix-VideoWall", QnAppServerConnectionFactory::videowallGuid().toString().toUtf8());

            requestUrl.setPath( lit("/ec2/%1").arg(ApiCommand::toString(cmdCode)) );
            QUrlQuery query;
            toUrlParams( input, &query );
            query.addQueryItem("format", QnLexical::serialized(Qn::UbjsonFormat));
            requestUrl.setQuery( query );

            connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QMutexLocker lk( &m_mutex );
            if( !httpClient->doGet( requestUrl ) )
            {
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, ErrorCode::failure, OutputData() ) );
                return;
            }
            auto func = std::bind( std::mem_fn( &ClientQueryProcessor::processHttpGetResponse<OutputData, HandlerType> ), this, httpClient, handler );
            m_runningHttpRequests[httpClient] = std::function<void()>( func );
        }

    public slots:
        void onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
        {
            std::function<void()> handler;
            {
                QMutexLocker lk( &m_mutex );
                auto it = m_runningHttpRequests.find( httpClient );
                assert( it != m_runningHttpRequests.end() );
                handler = std::move(it->second);
                httpClient->terminate();
                m_runningHttpRequests.erase( it );
            }

            handler();
        }

    private:
        QMutex m_mutex;
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
                case nx_http::StatusCode::unauthorized:
                    return handler( ErrorCode::unauthorized, OutputData() );
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
                assert(false);
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
