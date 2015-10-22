#pragma once

#include <functional>
#include "utils/network/http/httptypes.h"
#include "utils/common/systemerror.h"
#include "utils/common/request_param.h"
#include "nx_ec/data/api_fwd.h"
#include <api/helpers/request_helpers_fwd.h>

class QnNewMediaServerConnection
{
public:

    QnNewMediaServerConnection(const QnUuid& serverId);
    virtual ~QnNewMediaServerConnection() {}

    template <typename ResultType>
    struct Result { typedef std::function<void (bool, ResultType)> type; };

    bool cameraHistoryAsync(const QnChunksRequestData &request, Result<ec2::ApiCameraHistoryDataList>::type callback);
private:
    template <typename ResultType> bool execute(const QString& path, const QnRequestParamList& params, std::function<void (bool, ResultType)> callback) const;
    template <typename ResultType> bool execute(const QUrl& url, std::function<void (bool, ResultType)> callback) const;

    typedef std::function<void (SystemError::ErrorCode, int, nx_http::StringType contentType, nx_http::BufferType msgBody)> HttpCompletionFunc;
    bool sendRequest(const QnUuid& serverId, const QUrl& url, HttpCompletionFunc callback) const;
    QUrl prepareUrl(const QString& path, const QnRequestParamList& params) const;
private:
    QnUuid m_serverId;
};
