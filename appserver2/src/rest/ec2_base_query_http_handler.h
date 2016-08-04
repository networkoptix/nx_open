#pragma once

#include <QtCore/QByteArray>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <rest/server/request_handler.h>
#include <rest/server/rest_connection_processor.h>
#include <utils/common/concurrent.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/httptypes.h>

#include "ec2_thread_pool.h"
#include "request_params.h"
#include "server_query_processor.h"
#include "utils/common/util.h"

namespace ec2 {

// TODO: #MSAPI
//
// Think of inheriting this one from QnBasicRestHandler (ex-QnJsonRestHandler)
// and sharing the implementation of format handling.
//
// Btw, it would also make sense to do some renamings. This one is a
// rest handler, so should be named as such. ec2::BasicRestHandler?

/**
 * Http request handler for GET requests.
 */
template<class InputData, class OutputData, class Derived>
class BaseQueryHttpHandler:
    public QnRestRequestHandler
{
public:
    BaseQueryHttpHandler(ApiCommand::Value cmdCode): m_cmdCode(cmdCode) {}

    /**
     * Implementation of QnRestRequestHandler::executeGet().
     */
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override
    {
        InputData inputData;
        QString command = path.split(L'/').last();
        parseHttpRequestParams(command, params, &inputData);

        Qn::SerializationFormat format = Qn::JsonFormat;
        parseHttpRequestParams(command, params, &format);

        ErrorCode errorCode = ErrorCode::ok;
        bool finished = false;

        auto queryDoneHandler =
            [&](ErrorCode _errorCode, const OutputData& outputData)
            {
                if (_errorCode == ErrorCode::ok)
                {
                    if(format == Qn::UbjsonFormat)
                    {
                        result = QnUbjson::serialized(outputData);
                    }
                    else if (format == Qn::JsonFormat)
                    {
                        result = QJson::serialized(outputData);
                        if (params.contains("extraFormatting"))
                            result  = formatJSonString(result);
                    }
                    else if (format == Qn::CsvFormat)
                    {
                        result = QnCsv::serialized(outputData);
                    }
                    else if (format == Qn::XmlFormat)
                    {
                        result = QnXml::serialized(outputData, lit("reply"));
                    }
                    else
                    {
                        NX_ASSERT(false);
                    }
                }
                errorCode = _errorCode;
                contentType = Qn::serializationFormatToHttpContentType(format);

                QnMutexLocker lk(&m_mutex);
                finished = true;
                m_cond.wakeAll();
            };

        static_cast<Derived*>(this)->processQueryAsync(
            inputData,
            queryDoneHandler,
            owner);

        QnMutexLocker lk(&m_mutex);
        while(!finished)
            m_cond.wait(lk.mutex());

        return errorCode == ErrorCode::ok
            ? nx_http::StatusCode::ok
            : (errorCode == ErrorCode::unauthorized
                ? nx_http::StatusCode::unauthorized
                : nx_http::StatusCode::internalServerError);
    }

    /**
     * Implementation of QnRestRequestHandler::executePost() - stub.
     */
    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor*) override
    {
        QN_UNUSED(path, params, body, srcBodyContentType, result, contentType);
        return nx_http::StatusCode::badRequest;
    }

private:
    ApiCommand::Value m_cmdCode;
    QnWaitCondition m_cond;
    QnMutex m_mutex;
};


/**
 * Http request handler for GET requests.
 */
template<class InputData, class OutputData>
class QueryHttpHandler:
    public BaseQueryHttpHandler<InputData, OutputData, QueryHttpHandler<InputData, OutputData>>
{
    typedef BaseQueryHttpHandler<
        InputData, OutputData, QueryHttpHandler<InputData, OutputData>> base_type;

public:
    QueryHttpHandler(
        ApiCommand::Value cmdCode,
        ServerQueryProcessorAccess* const queryProcessor)
        :
        base_type(cmdCode),
        m_cmdCode(cmdCode),
        m_queryProcessor(queryProcessor)
    {
    }

    template<class HandlerType>
    void processQueryAsync(
        const InputData& inputData,
        HandlerType handler,
        const QnRestConnectionProcessor* owner)
    {
        m_queryProcessor->getAccess(Qn::UserAccessData(owner->authUserId()))
            .template processQueryAsync<InputData, OutputData, HandlerType>(
                m_cmdCode,
                inputData,
                handler);
    }

private:
    ApiCommand::Value m_cmdCode;
    ServerQueryProcessorAccess* const m_queryProcessor;
};



template<class InputData, class OutputData>
class FlexibleQueryHttpHandler:
    public BaseQueryHttpHandler<
        InputData, OutputData, FlexibleQueryHttpHandler<InputData, OutputData>>
{
    typedef BaseQueryHttpHandler<
        InputData, OutputData, FlexibleQueryHttpHandler<InputData, OutputData>> base_type;

public:
    FlexibleQueryHttpHandler(
        ApiCommand::Value cmdCode,
        std::function<ErrorCode(InputData, OutputData*)> queryHandler)
        :
        base_type(cmdCode),
        m_queryHandler(
            [queryHandler](
                InputData input, OutputData* output, nx_http::Response* response) -> ErrorCode
            {
                QN_UNUSED(response);
                return queryHandler(std::move(input), output);
            })
    {
    }

    FlexibleQueryHttpHandler(
        ApiCommand::Value cmdCode,
        std::function<ErrorCode(InputData, OutputData*, nx_http::Response*)> queryHandler)
        :
        base_type(cmdCode),
        m_queryHandler(std::move(queryHandler))
    {
    }

    template<class HandlerType>
    void processQueryAsync(
        const InputData& inputData,
        HandlerType handler,
        const QnRestConnectionProcessor* connection)
    {
        QnConcurrent::run(Ec2ThreadPool::instance(),
            [this, inputData, handler, connection]()
            {
                OutputData output;
                const ErrorCode errorCode = m_queryHandler(
                    inputData,
                    &output,
                    connection->response());
                handler(errorCode, output);
            });
    }

private:
    std::function<ErrorCode(InputData, OutputData*, nx_http::Response*)> m_queryHandler;
};

} // namespace ec2
