// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <Foundation/NSString.h>
#include <Foundation/NSURLResponse.h>

namespace extension::helpers {

long statusCodeFromResponse(NSURLResponse* response);

bool isSuccessHttpStatus(long statusCode);

/* Return cloud host for the current customization. */
NSURL* cloudHost();

/* Return URL by the applying specified path to the source URL. */
NSURL* urlFromHostAndPath(
    const NSURL* source,
    const NSString* path = nil);

enum class ContentType
{
    noContent,
    text,
    json
};

struct PostData
{
    ContentType type = ContentType::noContent;
    NSData* data = nil;
};

using SyncRequestCallback = std::function<void (long httpErrorCode, NSData* data)>;

/* Makes synchronous request with the specified url and data and calls callback at the end. */
void syncRequest(
    NSURL* url,
    const PostData& postData,
    SyncRequestCallback callback);

} // namespace extension::helpers
