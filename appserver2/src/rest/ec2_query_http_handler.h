/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_HTTP_REQUEST_HANDLER_H
#define EC2_HTTP_REQUEST_HANDLER_H

#include <condition_variable>
#include <mutex>

#include <QtCore/QByteArray>

#include <rest/server/request_handler.h>
#include <utils/network/http/httptypes.h>

#include "request_params.h"
#include "server_query_processor.h"


namespace ec2
{
    //!Http request handler for GET requests
    template<class InputData, class OutputData>
    class QueryHttpHandler
    :
        public QnRestRequestHandler
    {
    public:
        QueryHttpHandler(
            ServerQueryProcessor* const queryProcessor,
            ApiCommand::Value cmdCode )
        :
            m_queryProcessor( queryProcessor ),
            m_cmdCode( cmdCode )
        {
        }

        //!Implementation of QnRestRequestHandler::executeGet
        virtual int executeGet(
            const QString& /*path*/,
            const QnRequestParamList& params,
            QByteArray& result,
            QByteArray& contentType )
        {
            InputData inputData;
            parseHttpRequestParams( params, &inputData );

            ErrorCode errorCode = ErrorCode::ok;
            bool finished = false;

            auto queryDoneHandler = [&result, &errorCode, &contentType, &finished, this]( ErrorCode _errorCode, const OutputData& outputData )
            {
                if( _errorCode == ErrorCode::ok )
                {
                    OutputBinaryStream<QByteArray> stream( &result );
                    QnBinary::serialize( outputData, &stream );
                    contentType = "application/octet-stream";
                }
                errorCode = _errorCode;

                std::unique_lock<std::mutex> lk( m_mutex );
                finished = true;
                m_cond.notify_all();
            };
            m_queryProcessor->processQueryAsync<InputData, OutputData, decltype(queryDoneHandler)>(
                m_cmdCode,
                inputData,
                queryDoneHandler );

            std::unique_lock<std::mutex> lk( m_mutex );
            m_cond.wait( lk, [&finished]{ return finished; } );

            return errorCode == ErrorCode::ok
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }

        //!Implementation of QnRestRequestHandler::executePost
        virtual int executePost(
            const QString& /*path*/,
            const QnRequestParamList& /*params*/,
            const QByteArray& /*body*/,
            QByteArray& /*result*/,
            QByteArray& /*contentType*/ )
        {
            return nx_http::StatusCode::badRequest;
        }

        //!Implementation of QnRestRequestHandler::description
        virtual QString description() const
        {
            //TODO/IMPL
            return QString();
        }

    private:
        ServerQueryProcessor* const m_queryProcessor;
        ApiCommand::Value m_cmdCode;
        std::condition_variable m_cond;
        std::mutex m_mutex;
    };
}

#endif  //EC2_HTTP_REQUEST_HANDLER_H
