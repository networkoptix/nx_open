#pragma once

#include <QUrlQuery>

#include <utils/common/request_param.h>

struct QnMultiserverRequestData {
    QnMultiserverRequestData();

    template <typename T>
    static T fromParams(const QnRequestParamList& params) {
        T request;
        request.loadFromParams(params);
        return request;
    }

    virtual void loadFromParams(const QnRequestParamList& params);
    virtual QnRequestParamList toParams() const;
    virtual QUrlQuery toUrlQuery() const;
    virtual bool isValid() const;

    /* Fix fields to make local request. */
    void makeLocal(Qn::SerializationFormat localFormat = Qn::UbjsonFormat);

    bool isLocal;
    Qn::SerializationFormat format;
};
