/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef EC2_BASE_QUERY_HTTP_HANDLER_H
#define EC2_BASE_QUERY_HTTP_HANDLER_H

#include <QtCore/QByteArray>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QWaitCondition>
#include <QtConcurrent>

#include <rest/server/request_handler.h>
#include <utils/network/http/httptypes.h>

#include "request_params.h"
#include "server_query_processor.h"


namespace ec2
{
    //!Http request handler for GET requests
    template<class InputData, class OutputData, class ChildType>
    class BaseQueryHttpHandler
    :
        public QnRestRequestHandler
    {
    public:
        BaseQueryHttpHandler( ApiCommand::Value cmdCode )
        :
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
                    serialize( outputData, &stream );
                    contentType = "application/octet-stream";
                }
                errorCode = _errorCode;

                QMutexLocker lk( &m_mutex );
                finished = true;
                m_cond.wakeAll();
            };

            static_cast<ChildType*>(this)->template processQueryAsync<decltype(queryDoneHandler)>(
                m_cmdCode,
                inputData,
                queryDoneHandler );

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
        ApiCommand::Value m_cmdCode;
        QWaitCondition m_cond;
        QMutex m_mutex;
    };


    //!Http request handler for GET requests
    template<class InputData, class OutputData>
    class QueryHttpHandler2
    :
        public BaseQueryHttpHandler<InputData, OutputData, QueryHttpHandler2<InputData, OutputData> >
    {
    public:
        typedef BaseQueryHttpHandler<InputData, OutputData, QueryHttpHandler2<InputData, OutputData> > parent_type;

        QueryHttpHandler2(
            ApiCommand::Value cmdCode,
            ServerQueryProcessor* const queryProcessor )
        :
            parent_type( cmdCode ),
            m_cmdCode( cmdCode ),
            m_queryProcessor( queryProcessor )
        {
        }

        template<class HandlerType>
        void processQueryAsync( ApiCommand::Value /*cmdCode*/, const InputData& inputData, HandlerType handler )
        {
            // TODO: #Elric maybe compare m_cmdCode and passed cmdCode?
            m_queryProcessor->template processQueryAsync<InputData, OutputData, HandlerType>(
                m_cmdCode,
                inputData,
                handler );
        }

        //!Implementation of QnRestRequestHandler::description
        virtual QString description() const
        {
            //TODO/IMPL
            return QString();
        }

    private:
        ApiCommand::Value m_cmdCode;
        ServerQueryProcessor* const m_queryProcessor;
    };



    template<class InputData, class OutputData, class QueryHandlerType>
    class FlexibleQueryHttpHandler
    :
        public BaseQueryHttpHandler<InputData, OutputData, FlexibleQueryHttpHandler<InputData, OutputData, QueryHandlerType> >
    {
    public:
        typedef BaseQueryHttpHandler<InputData, OutputData, FlexibleQueryHttpHandler<InputData, OutputData, QueryHandlerType> > parent_type;

        FlexibleQueryHttpHandler( ApiCommand::Value cmdCode, QueryHandlerType queryHandler )
        :
            parent_type( cmdCode ),
            m_queryHandler( queryHandler )
        {
        }

        template<class HandlerType>
        void processQueryAsync( ApiCommand::Value /*cmdCode*/, const InputData& inputData, HandlerType handler )
        {
            QnScopedThreadRollback ensureFreeThread(1);
            QtConcurrent::run( [this, inputData, handler]() {
                OutputData output;
                const ErrorCode errorCode = m_queryHandler( inputData, &output );
                handler( errorCode, output );
            } );
        }

    private:
        QueryHandlerType m_queryHandler;
    };
}

#endif  //EC2_BASE_QUERY_HTTP_HANDLER_H
