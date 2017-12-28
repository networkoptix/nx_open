#pragma once

#include "request_handler.h"
#include "json_rest_result.h"

#include <nx/fusion/serialization/lexical_functions.h>

/**@file
 * TODO: #MSAPI
 *
 * Step1:
 * rename to something =). I suggest QnBasicRestHandler.
 *
 * Step2:
 * In get/post routines deserialize passed format.
 * Construct QnRestResult (ex-QnJsonRestResult) with the given format and pass it into the actual
 * get implementation. See json_rest_result.h for more.
 *
 * Step2.1:
 * You may end up with some of the result types not being (de)serializable to/from
 * our binary format. Make sure you've generated all the serializers. See
 * api_model_functions.cpp for more.
 *
 * Step3:
 * Think of inheriting ec2::BaseQueryHttpHandler from this one to share
 * the code. I think it is quite doable.
 *
 * Step4:
 * Inherit all MS API handlers from this one, with the exception
 * of the ones that use hand-rolled serialization. The only such handlers that
 * I'm aware of right now are QnLogRestHandler and QnRecordedChunksRestHandler.
 *
 * Step5:
 * Make sure QnRecordedChunksRestHandler adheres to the format interface.
 * E.g. it also has parameter 'format' and handles the same values for it,
 * and maybe some other values.
 */

struct JsonRestResponse
{
    nx_http::StatusCode::Value statusCode = nx_http::StatusCode::ok;
    QnJsonRestResult json;
    bool isUndefinedContentLength = false;

    JsonRestResponse(nx_http::StatusCode::Value statusCode = nx_http::StatusCode::undefined,
        QnJsonRestResult json = {}, bool isUndefinedContentLength = false);

    template<typename T>
    JsonRestResponse(const T& value) { json.setReply(value); }

    template<typename T>
    JsonRestResponse(QnJsonRestResult::Error error, const T& value) { json.setError(error, value); }

    JsonRestResponse(nx_http::StatusCode::Value status, QnJsonRestResult::Error error);

    RestResponse toRest(bool extraFormatting) const;
};

struct JsonRestRequest
{
    QString path;
    QnRequestParams params;
    const QnRestConnectionProcessor* owner = nullptr;

    JsonRestRequest(const RestRequest& rest);
    JsonRestRequest(QString path, const QnRequestParams& params, const QnRestConnectionProcessor* owner);
};

class QnJsonRestHandler: public QnRestRequestHandler
{
    Q_OBJECT

public:
    virtual ~QnJsonRestHandler() = default;

    // Interface to implement by inheritors:
    virtual JsonRestResponse executeGet(const JsonRestRequest& request);
    virtual JsonRestResponse executeDelete(const JsonRestRequest& request);
    virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);
    virtual JsonRestResponse executePut(const JsonRestRequest& request, const QByteArray& body);

    // Old style interface for compatability:
    virtual int executeGet(
        const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    virtual int executeDelete(
        const QString& path, const QnRequestParams& params, QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

    virtual int executePost(
        const QString& path, const QnRequestParams& params, const QByteArray& body,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner);

    virtual int executePut(
        const QString& path, const QnRequestParams& params, const QByteArray& body,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner);

protected:
    // QnRestRequestHandler interface implemenation:
    RestResponse executeGet(const RestRequest& request) override final;
    RestResponse executeDelete(const RestRequest& request) override final;
    RestResponse executePost(const RestRequest& request, const RestContent& content) override final;
    RestResponse executePut(const RestRequest& request, const RestContent& content) override final;

    // QnRestRequestHandler old interface locks:
    virtual int executeGet(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override final;

    virtual int executeDelete(
        const QString& path,
        const QnRequestParamList& params,
        QByteArray& result,
        QByteArray& contentType,
        const QnRestConnectionProcessor* owner) override final;

    virtual int executePost(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* owner) override final;

    virtual int executePut(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QByteArray& srcBodyContentType,
        QByteArray& result,
        QByteArray& resultContentType,
        const QnRestConnectionProcessor* owner) override final;

    template<class T>
    bool requireParameter(
        const QnRequestParams& params, const QString& key, QnJsonRestResult& result,
        T* value, bool optional = false) const
    {
        auto pos = params.find(key);
        if (pos == params.end())
        {
            if(optional)
            {
                return true;
            }
            else
            {
                result.setError(QnJsonRestResult::MissingParameter,
                    lit("Parameter '%1' is missing.").arg(key));
                return false;
            }
        }

        if (!QnLexical::deserialize(*pos, value))
        {
            result.setError(QnJsonRestResult::InvalidParameter,
                lit("Parameter '%1' has invalid value '%2'. Expected a value of type '%3'.")
                .arg(key).arg(*pos).arg(QLatin1String(typeid(T).name())));
            return false;
        }

        return true;
    }

private:
    QnRequestParams processParams(const QnRequestParamList& params) const;
};
