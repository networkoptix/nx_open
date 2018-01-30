#pragma once

#include <QUrlQuery>

#include <common/common_globals.h>

#include <utils/common/request_param.h>

class QnResourcePool;

struct QnBaseMultiserverRequestData
{
    static const Qn::SerializationFormat kDefaultFormat;

    bool isLocal = false; //< If set, the request should not be redirected to another server.
    Qn::SerializationFormat format = kDefaultFormat;
    bool extraFormatting = false;

    QnBaseMultiserverRequestData() = default;
    QnBaseMultiserverRequestData(const QnRequestParamList& params);
    QnBaseMultiserverRequestData(const QnRequestParams& params);

protected:
    void loadFromParams(const QnRequestParamList& params);
    void loadFromParams(const QnRequestParams& params);
};

struct QnMultiserverRequestData: QnBaseMultiserverRequestData
{
    virtual ~QnMultiserverRequestData();

    template<typename T, typename Params>
    static T fromParams(QnResourcePool* resourcePool, const Params& params)
    {
        T request;
        request.loadFromParams(resourcePool, params);
        return request;
    }

    virtual void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    virtual void loadFromParams(QnResourcePool* resourcePool, const QnRequestParams& params);
    virtual QnRequestParamList toParams() const;
    virtual QUrlQuery toUrlQuery() const;
    virtual bool isValid() const;

    /** Fix fields to make local request. */
    void makeLocal(Qn::SerializationFormat localFormat = Qn::UbjsonFormat);

protected:
    // Avoid creating invalid instances when making local requests.
    QnMultiserverRequestData() = default;
    QnMultiserverRequestData(const QnMultiserverRequestData& src) = default;
    QnMultiserverRequestData(QnMultiserverRequestData&& src) = default;
};

