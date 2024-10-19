// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "helpers.h"

#include <Foundation/NSURL.h>
#include <Foundation/NSURLSession.h>

#include "branding.h"

namespace extension::helpers {

NSURL* cloudHost()
{
    return extension::utils::kCloudHostUrl;
}

long statusCodeFromResponse(NSURLResponse* response)
{
    return [response isKindOfClass:[NSHTTPURLResponse class]]
        ? [(NSHTTPURLResponse *)response statusCode]
        : -1;
}

bool isSuccessHttpStatus(long statusCode)
{
    static constexpr long kMinSuccessCode = 200;
    static constexpr long kMaxSuccessCode = 299;

    return statusCode >= kMinSuccessCode && statusCode <= kMaxSuccessCode;
}

NSURL* urlFromHostAndPath(
    const NSURL* source,
    const NSString* path)
{
    NSString* stringUrl = path && [path length]
        ? [NSString stringWithFormat: @"%@://%@/%@", [source scheme], [source host], path]
        : [NSString stringWithFormat: @"%@://%@", [source scheme], [source host]];

    return [NSURL URLWithString: stringUrl];
}

void syncRequest(
    NSURL* url,
    const PostData& postData,
    SyncRequestCallback callback)
{
    bool handled = false;
    const auto semaphore = dispatch_semaphore_create(0);
    const auto resultHandler =
        [&semaphore, &handled, callback](
            NSData* _Nullable data,
            NSURLResponse* _Nullable response,
            NSError* _Nullable error)
        {
            handled = true;

            if (error)
                callback(-1, data);
            else
                callback(statusCodeFromResponse(response), data);

            dispatch_semaphore_signal(semaphore);
        };

    NSURLSession* session = [NSURLSession
        sessionWithConfiguration: [NSURLSessionConfiguration defaultSessionConfiguration]
        delegate: nil
        delegateQueue: nil];

    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL: url];
    if (postData.data && postData.type != ContentType::noContent)
    {
        const auto contentTypeString = postData.type == ContentType::json
            ? @"application/json"
            : @"text/plain";
        [request setValue: contentTypeString forHTTPHeaderField: @"Content-Type"];
        [request setHTTPMethod: @"POST"];
        [request setHTTPBody: postData.data];
    }

    const NSURLSessionDataTask* task =
        [session dataTaskWithRequest: request completionHandler: resultHandler];
    [task resume];

    static constexpr int64_t kTimeout = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::seconds(10)).count();
    if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, kTimeout)))
        [task cancel];

    if (!handled)
        callback(-1, nullptr);
}

} // namespace extension::helpers
