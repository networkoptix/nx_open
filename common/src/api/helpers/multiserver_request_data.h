#pragma once

#include <QUrlQuery>

#include <common/common_globals.h>

#include <utils/common/request_param.h>

struct QnMultiserverRequestData
{
    template<typename T, typename Params>
    static T fromParams(const Params& params)
    {
        T request;
        request.loadFromParams(params);
        return request;
    }

    explicit QnMultiserverRequestData(const QnRequestParamList& params);

    virtual void loadFromParams(const QnRequestParamList& params);
    virtual void loadFromParams(const QnRequestParams& params);
    virtual QnRequestParamList toParams() const;
    virtual QUrlQuery toUrlQuery() const;
    virtual bool isValid() const;

    /** Fix fields to make local request. */
    void makeLocal(Qn::SerializationFormat localFormat = Qn::UbjsonFormat);

    bool isLocal; //< Shows if this request is sent by redirection.
    Qn::SerializationFormat format;
    bool extraFormatting;

protected:
    // Avoid creating invalid instances when making local requests.
    QnMultiserverRequestData();
    QnMultiserverRequestData(const QnMultiserverRequestData& src);
};
