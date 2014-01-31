/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef CLIENT_QUERY_PROCESSOR_H
#define CLIENT_QUERY_PROCESSOR_H

#include <map>
#include <mutex>

#include <QtConcurrent>
#include <QtCore/QObject>

#include <utils/network/http/asynchttpclient.h>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "rest/request_params.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"


namespace ec2
{
    class ClientQueryProcessor
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~ClientQueryProcessor() {}

        //!Asynchronously executes update query
        /*!
            \param handler Functor ( ErrorCode )
        */
        template<class QueryDataType, class HandlerType>
            void processUpdateAsync( const QnTransaction<QueryDataType>& /*tran*/, HandlerType handler )
        {
            //TODO/IMPL
            QtConcurrent::run( std::bind( handler, ErrorCode::failure ) );
        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO allow compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand::Value cmdCode, InputData input, HandlerType handler )
        {
            QUrl requestUrl( m_ecBaseUrl );
            requestUrl.setPath( QString::fromLatin1("ec2/%1").arg(ApiCommand::toString(cmdCode)) );
            QUrlQuery query;
            toUrlParams( input, &query );
            requestUrl.setQuery( query );

            nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
            connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );
            {
                std::unique_lock<std::mutex> lk( m_mutex );
                if( !httpClient->doGet( requestUrl ) )
                {
                    QtConcurrent::run( std::bind( handler, ErrorCode::failure, OutputData() ) );
                    return;
                }
                auto func = std::bind( std::mem_fn( &ClientQueryProcessor::processHttpResponse<OutputData, HandlerType> ), this, httpClient, handler );
                m_runningHttpRequests[httpClient] = new CustomHandler<decltype(func)>(func);
            }
        }

    public slots:
        void onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
        {
            std::unique_ptr<AbstractHandler> handler;
            {
                std::unique_lock<std::mutex> lk( m_mutex );
                auto it = m_runningHttpRequests.find( httpClient );
                assert( it != m_runningHttpRequests.end() );
                handler.reset( it->second );
                m_runningHttpRequests.erase( it );
            }

            handler->doIt();
        }
         
    private:
        class AbstractHandler
        {
        public:
            virtual ~AbstractHandler() {}
            virtual void doIt() = 0;
        };

        template<class Handler>
        class CustomHandler : public AbstractHandler
        {
        public:
            CustomHandler( const Handler& handler ) : m_handler( handler ) {}
            virtual void doIt() override { m_handler(); }

        private:
            Handler m_handler;
        };

        QUrl m_ecBaseUrl;
        std::mutex m_mutex;
        std::map<nx_http::AsyncHttpClientPtr, AbstractHandler*> m_runningHttpRequests;

        template<class OutputData, class HandlerType>
            void processHttpResponse( nx_http::AsyncHttpClientPtr httpClient, HandlerType handler )
        {
            if( httpClient->failed() || !httpClient->response() )
                return handler( ErrorCode::ioError, OutputData() );
            switch( httpClient->response()->statusLine.statusCode )
            {
                case nx_http::StatusCode::ok:
                    break;

                default:
                    return handler( ErrorCode::serverError, OutputData() );
            }

            InputBinaryStream<QByteArray> inputStream( httpClient->response()->messageBody );
            OutputData outputData;
            QnBinary::deserialize( outputData, &inputStream );
            return handler( ErrorCode::ok, outputData );
        }
    };
}

#endif  //CLIENT_QUERY_PROCESSOR_H
