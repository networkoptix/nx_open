// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrlQuery>

#include <common/common_globals.h>
#include <nx/utils/serialization/format.h>

class QnResourcePool;
namespace nx::network::rest { class Params; }

struct NX_VMS_COMMON_API QnBaseMultiserverRequestData
{
    static const Qn::SerializationFormat kDefaultFormat;

    bool isLocal = false; //< If set, the request should not be redirected to another server.
    Qn::SerializationFormat format = kDefaultFormat;

    QnBaseMultiserverRequestData() = default;
    QnBaseMultiserverRequestData(const nx::network::rest::Params& params);
    virtual nx::network::rest::Params toParams() const;

protected:
    void loadFromParams(const nx::network::rest::Params& params);

private:
    bool extraFormatting = false;
};

struct NX_VMS_COMMON_API QnMultiserverRequestData: QnBaseMultiserverRequestData
{
    virtual ~QnMultiserverRequestData();

    template<typename T, typename Params>
    static T fromParams(QnResourcePool* resourcePool, const Params& params)
    {
        T request;
        request.loadFromParams(resourcePool, params);
        return request;
    }

    virtual void loadFromParams(QnResourcePool* resourcePool, const nx::network::rest::Params& params);
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
