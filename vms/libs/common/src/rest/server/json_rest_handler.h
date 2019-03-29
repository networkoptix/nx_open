#pragma once

#include "request_handler.h"
#include "json_rest_result.h"

#include <nx/fusion/serialization/lexical_functions.h>
#include <api/model/password_data.h>

// TODO: Get rid in favor if direct usage of QnRestRequestHandler.
class QnJsonRestHandler: public QnRestRequestHandler
{
protected:
    virtual RestResponse executeGet(const RestRequest& request) override;
    virtual RestResponse executeDelete(const RestRequest& request) override;
    virtual RestResponse executePost(const RestRequest& request) override;
    virtual RestResponse executePut(const RestRequest& request) override;

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

    // TODO: Rewrite on exceptions???
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

    template<class T>
    void copyParameter(
        const QnRequestParams& params,
        const QString& key,
        QnJsonRestResult& result,
        T* value) const
    {
        requireParameter(params, key, result, value, /*optional*/ true);
    }

    template<class T>
    bool requireOneOfParameters(
        const QnRequestParams& params,
        const QStringList& keys,
        QnJsonRestResult& result,
        T* value,
        bool isOptional = false) const
    {
        bool success = false;
        for (const auto& key: keys)
        {
            success = requireParameter(params, key, result, value, false);
            if (success)
            {
                result.setError(QnJsonRestResult::NoError);
                return true;
            }
        }

        return isOptional;
    }
};
