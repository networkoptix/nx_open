/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef CLIENT_QUERY_PROCESSOR_H
#define CLIENT_QUERY_PROCESSOR_H

#include <map>

#include <QtConcurrent>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>

#include <api/app_server_connection.h>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/network/http/asynchttpclient.h>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "rest/request_params.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"
#include "utils/serialization/binary_functions.h"

namespace ec2
{
    class ClientQueryProcessor
    :
        public QObject
    {
        Q_OBJECT

    public:
        virtual ~ClientQueryProcessor()
        {
            assert( m_runningHttpRequests.empty() );
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
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }

            requestUrl.setPath( QString::fromLatin1("/ec2/%1").arg(ApiCommand::toString(tran.command)) );

            QByteArray tranBuffer;
            QnOutputBinaryStream<QByteArray> outputStream( &tranBuffer );
            QnBinary::serialize( tran, &outputStream );

            connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QMutexLocker lk( &m_mutex );
            if( !httpClient->doPost( requestUrl, "application/octet-stream", tranBuffer ) )
            {
                QnScopedThreadRollback ensureFreeThread(1);
                QtConcurrent::run( std::bind( handler, ErrorCode::failure ) );
                return;
            }
            //auto func = std::bind( std::mem_fn( &ClientQueryProcessor::processHttpPostResponse<HandlerType> ), this, httpClient, handler );
            auto func = [this, httpClient, handler](){ processHttpPostResponse( httpClient, handler ); };
            m_runningHttpRequests[httpClient] = new CustomHandler<decltype(func)>(func);
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
            if (!requestUrl.userName().isEmpty()) {
                httpClient->setUserName(requestUrl.userName());
                httpClient->setUserPassword(requestUrl.password());
                requestUrl.setUserName(QString());
                requestUrl.setPassword(QString());
            }
            if (!QnAppServerConnectionFactory::videoWallKey().isEmpty())
                httpClient->addRequestHeader("X-NetworkOptix-VideoWall", QnAppServerConnectionFactory::videoWallKey().toUtf8());

            requestUrl.setPath( QString::fromLatin1("/ec2/%1").arg(ApiCommand::toString(cmdCode)) );
            QUrlQuery query;
            toUrlParams( input, &query );
            requestUrl.setQuery( query );

            connect( httpClient.get(), &nx_http::AsyncHttpClient::done, this, &ClientQueryProcessor::onHttpDone, Qt::DirectConnection );

            QMutexLocker lk( &m_mutex );
            if( !httpClient->doGet( requestUrl ) )
            {
                QnScopedThreadRollback ensureFreeThread(1);
                QtConcurrent::run( std::bind( handler, ErrorCode::failure, OutputData() ) );
                return;
            }
            auto func = std::bind( std::mem_fn( &ClientQueryProcessor::processHttpGetResponse<OutputData, HandlerType> ), this, httpClient, handler );
            m_runningHttpRequests[httpClient] = new CustomHandler<decltype(func)>(func);
        }

        template<class T> bool processIncomingTransaction( const QnTransaction<T>&, const QByteArray&   )
        {
            // nothing to do for a while
            return true;
        }

    public slots:
        void onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
        {
            std::unique_ptr<AbstractHandler> handler;
            {
                QMutexLocker lk( &m_mutex );
                auto it = m_runningHttpRequests.find( httpClient );
                assert( it != m_runningHttpRequests.end() );
                handler.reset( it->second );
                httpClient->terminate();
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

        QMutex m_mutex;
        std::map<nx_http::AsyncHttpClientPtr, AbstractHandler*> m_runningHttpRequests;

        template<class OutputData, class HandlerType>
            void processHttpGetResponse( nx_http::AsyncHttpClientPtr httpClient, HandlerType handler )
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

            const QByteArray& msgBody = httpClient->fetchMessageBodyBuffer();
            QnInputBinaryStream<QByteArray> inputStream( msgBody );
            OutputData outputData;
            if( !QnBinary::deserialize( &inputStream, &outputData ) )
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
                    handler( ErrorCode::ok );
                    break;

                case nx_http::StatusCode::unauthorized:
                    handler( ErrorCode::unauthorized );
                    break;

                default:
                    return handler( ErrorCode::serverError );
            }
        }
    };
}

#endif  //CLIENT_QUERY_PROCESSOR_H
