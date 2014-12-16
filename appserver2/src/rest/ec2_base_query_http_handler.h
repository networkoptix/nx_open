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

#include <rest/server/request_handler.h>
#include <utils/common/concurrent.h>
#include <utils/common/model_functions.h>
#include <utils/network/http/httptypes.h>

#include "ec2_thread_pool.h"
#include "request_params.h"
#include "server_query_processor.h"
#include "utils/common/util.h"


namespace ec2
{
    // TODO: #MSAPI 
    //
    // Think of inheriting this one from QnBasicRestHandler (ex-QnJsonRestHandler)
    // and sharing the implementation of format handling.
    // 
    // Btw, it would also make sense to do some renamings. This one is a 
    // rest handler, so should be named as such. ec2::BasicRestHandler?
    // 
    //

    //!Http request handler for GET requests
    template<class InputData, class OutputData, class Derived>
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
            const QString& path,
            const QnRequestParamList& params,
            QByteArray& result,
            QByteArray& contentType,
            const QnRestConnectionProcessor*) override
        {
            InputData inputData;
            QString command = path.split(L'/').last();
            parseHttpRequestParams( command, params, &inputData);

            Qn::SerializationFormat format = Qn::JsonFormat;
            parseHttpRequestParams( command, params, &format);

            ErrorCode errorCode = ErrorCode::ok;
            bool finished = false;

            auto queryDoneHandler = [&]( ErrorCode _errorCode, const OutputData& outputData )
            {
                if( _errorCode == ErrorCode::ok )
                {
                    if(format == Qn::UbjsonFormat) {
                        result = QnUbjson::serialized(outputData);
                    //} else if(format == Qn::BnsFormat) {
                    //    result = QnBinary::serialized(outputData);
                    } else if(format == Qn::JsonFormat) {
                        result = QJson::serialized(outputData);
                        if (params.contains("extraFormatting"))
                            result  = formatJSonString(result);
                    } else if(format == Qn::CsvFormat) {
                        result = QnCsv::serialized(outputData);
                    } else if(format == Qn::XmlFormat) {
                        result = QnXml::serialized(outputData, lit("reply"));
                    } else {
                        assert(false);
                    }
                }
                errorCode = _errorCode;
                contentType = Qn::serializationFormatToHttpContentType(format);

                QMutexLocker lk( &m_mutex );
                finished = true;
                m_cond.wakeAll();
            };

            static_cast<Derived*>(this)->processQueryAsync(
                inputData,
                queryDoneHandler );

            QMutexLocker lk( &m_mutex );
            while( !finished )
                m_cond.wait( lk.mutex() );

            return errorCode == ErrorCode::ok
                ? nx_http::StatusCode::ok
                : (errorCode == ErrorCode::unauthorized
                   ? nx_http::StatusCode::unauthorized
                   : nx_http::StatusCode::internalServerError);
        }

        //!Implementation of QnRestRequestHandler::executePost
        virtual int executePost(
            const QString& /*path*/,
            const QnRequestParamList& /*params*/,
            const QByteArray& /*body*/,
            const QByteArray& /*srcBodyContentType*/,
            QByteArray& /*result*/,
            QByteArray&, /*contentType*/ 
            const QnRestConnectionProcessor*) override
        {
            return nx_http::StatusCode::badRequest;
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
        typedef BaseQueryHttpHandler<InputData, OutputData, QueryHttpHandler2<InputData, OutputData> > base_type;

    public:
        QueryHttpHandler2(
            ApiCommand::Value cmdCode,
            ServerQueryProcessor* const queryProcessor )
        :
            base_type( cmdCode ),
            m_cmdCode( cmdCode ),
            m_queryProcessor( queryProcessor )
        {
        }

        template<class HandlerType>
        void processQueryAsync( const InputData& inputData, HandlerType handler )
        {
            m_queryProcessor->template processQueryAsync<InputData, OutputData, HandlerType>(
                m_cmdCode,
                inputData,
                handler );
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
        typedef BaseQueryHttpHandler<InputData, OutputData, FlexibleQueryHttpHandler<InputData, OutputData, QueryHandlerType> > base_type; 

    public:
        FlexibleQueryHttpHandler( ApiCommand::Value cmdCode, QueryHandlerType queryHandler )
        :
            base_type( cmdCode ),
            m_queryHandler( queryHandler )
        {
        }

        template<class HandlerType>
        void processQueryAsync( const InputData& inputData, HandlerType handler )
        {
            QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
            QnConcurrent::run( Ec2ThreadPool::instance(),
                [this, inputData, handler]() {
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
