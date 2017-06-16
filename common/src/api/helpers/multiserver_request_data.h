#pragma once

#include <QUrlQuery>

#include <common/common_globals.h>

#include <utils/common/request_param.h>

class QnResourcePool;

struct QnMultiserverRequestData
{
    template<typename T, typename Params>
    static T fromParams(QnResourcePool* resourcePool, const Params& params)
    {
        T request;
        request.loadFromParams(resourcePool, params);
        return request;
    }

    QnMultiserverRequestData(QnResourcePool* resourcePool, const QnRequestParamList& params);

    virtual void loadFromParams(QnResourcePool* resourcePool, const QnRequestParamList& params);
    virtual void loadFromParams(QnResourcePool* resourcePool, const QnRequestParams& params);
    virtual QnRequestParamList toParams() const;
    virtual QUrlQuery toUrlQuery() const;
    virtual bool isValid() const;

    /** Fix fields to make local request. */
    void makeLocal(Qn::SerializationFormat localFormat = Qn::UbjsonFormat);

    bool isLocal; //< Whether the request was sent via redirection.
    Qn::SerializationFormat format;
    bool extraFormatting;

protected:
    // Avoid creating invalid instances when making local requests.
    QnMultiserverRequestData();
    QnMultiserverRequestData(const QnMultiserverRequestData& src);
};
