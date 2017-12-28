#pragma once

#include <QtCore/QByteArray>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <rest/server/request_handler.h>
#include <rest/server/rest_connection_processor.h>
#include <nx/utils/concurrent.h>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>

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
                    result = Qn::serialized(
                        outputData, format, params.contains("extraFormatting"));
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

        switch (errorCode)
        {
            case ErrorCode::ok:
                return nx::network::http::StatusCode::ok;
            case ErrorCode::unauthorized:
                return nx::network::http::StatusCode::unauthorized;
            case ErrorCode::forbidden:
                return nx::network::http::StatusCode::forbidden;
            default:
                return nx::network::http::StatusCode::internalServerError;
        }
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
        return nx::network::http::StatusCode::badRequest;
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
        m_queryProcessor->getAccess(owner->accessRights())
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
        std::function<ErrorCode(InputData, OutputData*, const Qn::UserAccessData&)> queryHandler)
        :
        base_type(cmdCode),
        m_queryHandler(
            [queryHandler](
                InputData input, OutputData* output, const Qn::UserAccessData& accessData, nx::network::http::Response* response) -> ErrorCode
            {
                QN_UNUSED(response);
                return queryHandler(std::move(input), output, accessData);
            })
    {
    }

    FlexibleQueryHttpHandler(
        ApiCommand::Value cmdCode,
        std::function<ErrorCode(InputData, OutputData*, nx::network::http::Response*)> queryHandler)
        :
        base_type(cmdCode)
    {
        m_queryHandler = [queryHandler](InputData input, OutputData* output, const Qn::UserAccessData&, nx::network::http::Response* response)
        {
            return queryHandler(input, output, response);
        };
    }

    template<class HandlerType>
    void processQueryAsync(
        const InputData& inputData,
        HandlerType handler,
        const QnRestConnectionProcessor* connection)
    {
        nx::utils::concurrent::run(Ec2ThreadPool::instance(),
            [this, inputData, handler, connection]()
            {
                OutputData output;
                const ErrorCode errorCode = m_queryHandler(
                    inputData,
                    &output,
                    connection->accessRights(),
                    connection->response());
                handler(errorCode, output);
            });
    }

private:
    std::function<ErrorCode(InputData, OutputData*, const Qn::UserAccessData& accessData, nx::network::http::Response*)> m_queryHandler;
};

} // namespace ec2
