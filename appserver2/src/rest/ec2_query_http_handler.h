/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_HTTP_REQUEST_HANDLER_H
#define EC2_HTTP_REQUEST_HANDLER_H

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>

#include <rest/server/request_handler.h>
#include <utils/network/http/httptypes.h>

#include "request_params.h"
#include "server_query_processor.h"


namespace ec2
{

    // TODO: #MSAPI Remove? This one seem not to be not used. Ask Andrey first.

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
            
            Qn::SerializationFormat format = Qn::BnsFormat;
            parseHttpRequestParams( params, &format );

            ErrorCode errorCode = ErrorCode::ok;
            bool finished = false;

            auto queryDoneHandler = [&]( ErrorCode _errorCode, const OutputData& outputData )
            {
                if( _errorCode == ErrorCode::ok )
                {
                    if(format == Qn::BnsFormat) {
                        result = QnBinary::serialized(outputData);
                        contentType = "application/octet-stream";
                    } else if(format == Qn::JsonFormat) {
                        result = QJson::serialized(outputData);
                        contentType = "application/json";
                    } else {
                        assert(false);
                    }
                }
                errorCode = _errorCode;

                QMutexLocker lk( &m_mutex );
                finished = true;
                m_cond.wakeAll();
            };
            m_queryProcessor->template processQueryAsync<InputData, OutputData>(m_cmdCode, inputData, queryDoneHandler);

            QMutexLocker lk( &m_mutex );
            while( !finished )
                m_cond.wait( lk.mutex() );

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
        QWaitCondition m_cond;
        QMutex m_mutex;
    };
}

#endif  //EC2_HTTP_REQUEST_HANDLER_H
