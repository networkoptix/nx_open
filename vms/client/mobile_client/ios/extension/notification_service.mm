// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_service.h"

#include <UIKit/UIKit.h>
#include <MobileCoreServices/MobileCoreServices.h>
#include <UserNotifications/UserNotifications.h>

#include <chrono>

#include <nx/vms/client/mobile/push_notification/details/push_ipc_data.h>

#include "utils/helpers.h"
#include "utils/logger.h"

using PushIpcData = nx::vms::client::mobile::details::PushIpcData;

namespace {

static const auto kAccessErrorText = @"No access to this information.";
static const int kNotFoundHttpError = 404;

using namespace extension::helpers;

enum class AuthMethod
{
    invalid,
    digest,
    bearer
};

enum class AccessTokenStatus
{
    unsupported,
    invalid,
    valid
};

NSDictionary* extractValuesFromResponse(
    int httpErrorCode,
    NSData* data,
    const NSString* logTag)
{
    if (!isSuccessHttpStatus(httpErrorCode))
    {
        extension::Logger::log(logTag, [NSString stringWithFormat:
            @"can't request access token: HTTP code is %d.", httpErrorCode]);
        return nullptr;
    }

    if (!data)
    {
        extension::Logger::log(logTag, @"can't process access token response: data is null.");
        return nullptr;
    }

    NSError* parseError = nil;
    NSDictionary* values = [NSJSONSerialization JSONObjectWithData: data
        options: NSJSONReadingMutableContainers error: &parseError];

    if (parseError || !values)
    {
        extension::Logger::log(logTag, @"can't process access token response: data is corrupted.");
        return nullptr;
    }

    return values;
}

int getIntValue(NSDictionary* dict, NSString* key)
{
    id value = [dict objectForKey: key];
    return value
        ? [value integerValue]
        : -1;
}

AccessTokenStatus systemAccessTokenStatus(
    const std::string& token,
    const NSURL* serverUrl)
{
    static const NSString* kLogTag = @"access token check";

    extension::Logger::log(kLogTag, @"started.");

    if (token.empty())
    {
        extension::Logger::log(kLogTag, @"token is empty, status is invalid.");
        return AccessTokenStatus::invalid;
    }

    AccessTokenStatus result = AccessTokenStatus::invalid;
    const auto handleReply =
        [&result](int httpErrorCode, NSData* data)
        {
            extension::Logger::log(kLogTag, [NSString stringWithFormat: @"HTTP code is %d.", httpErrorCode]);

            if (isSuccessHttpStatus(httpErrorCode))
            {
                NSString* message = @"";

                if (NSDictionary* values = extractValuesFromResponse(httpErrorCode, data, kLogTag))
                {
                    NSString* resultToken = values[@"token"];
                    static constexpr int kMinTokenLength = 6;
                    if (resultToken && resultToken.length >= kMinTokenLength)
                    {
                        message = [NSString stringWithFormat: @"****%@ (expires in: %d, age: %d) ",
                            [resultToken substringFromIndex: [resultToken length] - kMinTokenLength],
                            getIntValue(values, @"expiresInS"), getIntValue(values, @"ageS")];
                    }
                }
                extension::Logger::log(kLogTag, [NSString stringWithFormat: @"token %@is valid.", message]);
                result = AccessTokenStatus::valid;
            }
            else if (httpErrorCode == kNotFoundHttpError)
            {
                extension::Logger::log(kLogTag, @"token functionality is not supported.");
                result = AccessTokenStatus::unsupported;
            }
            else
            {
                NSString* message = @"";
                if (data)
                {
                    message = [NSString stringWithFormat: @", response is %@",
                        [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding]];
                }
                extension::Logger::log(kLogTag, [NSString stringWithFormat: @"token in invalid%@", message]);
                result = AccessTokenStatus::invalid;
            }
        };

    NSString* path = [NSString stringWithFormat: @"rest/v1/login/sessions/%s", token.c_str()];
    NSURL* url = urlFromHostAndPath(serverUrl, path);
    syncRequest(url, /*postData*/ {}, handleReply);

    extension::Logger::log(kLogTag, @"finished.");
    return result;
}

PushIpcData::TokenInfo requestSystemAccessToken(
    const std::string& user,
    const std::string& cloudRefreshToken,
    const std::string& cloudSystemId)
{
    static const NSString* kLogTag = @"access token request";

    extension::Logger::log(kLogTag, @"started.");

    PushIpcData::TokenInfo result;

    const auto extractToken =
        [&result](int httpErrorCode, NSData* data)
        {
            NSDictionary* values = extractValuesFromResponse(httpErrorCode, data, kLogTag);
            if (!values)
                return;

            NSString* tokenValue = [NSString stringWithString: values[@"access_token"]];
            if (!tokenValue || ![tokenValue length])
            {
                extension::Logger::log(kLogTag, @"can't handle response: token value was not found.");
                return;
            }

            NSString* expiresAtStr = [values objectForKey:@"expires_at"];

            result.accessToken = std::string([tokenValue UTF8String]);
            if (expiresAtStr)
                result.expiresAt = std::chrono::milliseconds([expiresAtStr longLongValue]);
            extension::Logger::log(kLogTag, @"access token was created.");
        };

    NSString* bodyString = [NSString stringWithFormat:
        @"{\
            \"grant_type\": \"refresh_token\",\
            \"response_type\": \"token\",\
            \"refresh_token\": \"%@\",\
            \"scope\": \"cloudSystemId=%@\",\
            \"username\": \"%@\"\
        }",
        @(cloudRefreshToken.c_str()),
        @(cloudSystemId.c_str()),
        @(user.c_str())];

    NSURL* url = urlFromHostAndPath(extension::helpers::cloudHost(), @"cdb/oauth2/token");
    NSData* body = [bodyString dataUsingEncoding: NSUTF8StringEncoding];
    syncRequest(url, {ContentType::json, body}, extractToken);
    return result;
}

NSURLSession* createSession(
    id delegate,
    const std::string& cloudSystemId,
    AuthMethod method)
{
    switch (method)
    {
        case AuthMethod::digest:
        {
            return [NSURLSession
                sessionWithConfiguration: [NSURLSessionConfiguration defaultSessionConfiguration]
                delegate: delegate
                delegateQueue: [NSOperationQueue mainQueue]];
        }
        case AuthMethod::bearer:
        {
            NSURLSessionConfiguration* config =
                [NSURLSessionConfiguration defaultSessionConfiguration];

            const auto tokenInfo = PushIpcData::accessToken(cloudSystemId);
            NSString* authString =
                [NSString stringWithFormat: @"Bearer %s", tokenInfo.accessToken.c_str()];

            [config setHTTPAdditionalHeaders: @{@"Authorization": authString}];

            return [NSURLSession
                sessionWithConfiguration: config
                delegate: nil
                delegateQueue: nil];
        }
        default:
            return nil;
    }
}

UNNotificationAttachment* makeAttachment(NSURL* source, const NSString* logTag)
{
    NSError* error = nil;
    NSURL* imageUrl = nil;
    const auto fileManager = [NSFileManager defaultManager];
    const auto uniqueName = [[NSProcessInfo processInfo] globallyUniqueString];
    const auto filename = [NSString stringWithFormat: @"%@%@", uniqueName, @".png"];
    imageUrl = [[fileManager temporaryDirectory] URLByAppendingPathComponent: filename];
    [fileManager moveItemAtURL: source toURL: imageUrl error: &error];

    if (error)
    {
        extension::Logger::log(logTag, @"can't save image.");
        return nil;
    }

    const auto result = [UNNotificationAttachment
        attachmentWithIdentifier:@""
        URL: imageUrl
        options: nil
        error: nil];

    if (!result)
    {
        extension::Logger::log(
            logTag, @"can't create attachments, most likely image is empty or invalid.");
    }

    return result;
}

}

using ContentHanlder = void (^)(UNNotificationContent* content);

@interface NotificationService () <NSURLSessionTaskDelegate>
@property(nonatomic) ContentHanlder handler;
@property(nonatomic) UNMutableNotificationContent* content;
@property(nonatomic) std::string user;
@property(nonatomic) std::string password;
@property(nonatomic) std::string cloudRefreshToken;
@property(nonatomic) std::string cloudSystemId;
- (bool) initialize: (NSString*) targets;
- (void) showNotification: (NSString*) imageUrl;
- (void) showNotificatoinWithImage: (NSString*) imageUrl withMethod: (AuthMethod) method;
@end

@implementation NotificationService

- (void) didReceiveNotificationRequest: (UNNotificationRequest*) request
    withContentHandler: (void (^)(UNNotificationContent* _Nonnull)) contentHandler
{
    extension::Logger::log(@"notification received");

    self.content = [request.content mutableCopy];
    [self.content setSound: [UNNotificationSound defaultSound]];

    self.handler = contentHandler;

    const auto data = [self.content userInfo];

    const bool allowNotification = [self initialize: data[@"targets"]];
    self.cloudSystemId = std::string([data[@"systemId"] UTF8String]);

    if (allowNotification)
    {
        extension::Logger::log(@"notification data is accessible");
    }
    else
    {
        extension::Logger::log(@"warning, access to notification data is restricted.");
        self.content.title = kAccessErrorText;
        self.content.body = @"";
    }

    if (allowNotification)
        [self showNotification: data[@"imageUrl"]];
    else //< Since we can't prevent notification popup in iOS we just show it with restricted data.
        [self serviceExtensionTimeWillExpire];
}

- (void)serviceExtensionTimeWillExpire
{
    extension::Logger::log(@"show standard notification");
    self.handler(self.content);
}

- (bool) initialize: (NSString*) targets
{
    static const NSString* kLogTag = @"initialization";

    if (!targets)
    {
        extension::Logger::log(kLogTag, @"empty targets, discarding notification");
        return false;
    }

    std::string user, password, cloudRefreshToken;
    if (!PushIpcData::load(user, cloudRefreshToken, password))
    {
        extension::Logger::log(kLogTag,
            @"can't load current keychain user data, discarding notification");
        return false;
    }

    if (user.empty() || (password.empty() && cloudRefreshToken.empty()))
    {
        extension::Logger::log(kLogTag, @"not enough credentials data, discarding notification");
        return false;
    }

    self.user = user;
    self.password = password;
    self.cloudRefreshToken = cloudRefreshToken;

    const auto searchString = [NSString stringWithFormat: @"\"%s\"", self.user.c_str()];
    const bool isInTargets = [targets containsString: searchString];
    if (!isInTargets)
    {
        extension::Logger::log(kLogTag,
            @"current user is not found in targets, discarding notificiation");
    }

    return isInTargets;
}

- (void)showNotification: (NSString*) imageUrl
{
    auto tokenInfo = PushIpcData::accessToken(self.cloudSystemId);

    const NSURL* serverUrl = urlFromHostAndPath([NSURL URLWithString: imageUrl], @"");
    AccessTokenStatus status = systemAccessTokenStatus(tokenInfo.accessToken, serverUrl);

    if (status == AccessTokenStatus::unsupported)
    {
        // 4.2 and below servers do not support token authorization.
        [self showNotificatoinWithImage: imageUrl withMethod: AuthMethod::digest];
        return;
    }

    if (status == AccessTokenStatus::invalid)
    {
        tokenInfo = requestSystemAccessToken(self.user, self.cloudRefreshToken, self.cloudSystemId);

        status = systemAccessTokenStatus(tokenInfo.accessToken, serverUrl);
        if (!tokenInfo.accessToken.empty() && status != AccessTokenStatus::valid)
        {
            extension::Logger::log(@"resetting new access token since it is not valid.");
            tokenInfo = {}; //< Most likely it is server with version < 5.0.
        }
    }

    PushIpcData::setAccessToken(self.cloudSystemId, tokenInfo.accessToken, tokenInfo.expiresAt);

    if (!tokenInfo.accessToken.empty()) //< Definitely supports OAuth.
        [self showNotificatoinWithImage: imageUrl withMethod: AuthMethod::bearer];
    else if (status == AccessTokenStatus::unsupported) //< Try to use digest authorization.
        [self showNotificatoinWithImage: imageUrl withMethod: AuthMethod::digest];
    else
        [self serviceExtensionTimeWillExpire];
}

- (void)showNotificatoinWithImage: (NSString*) imageUrl withMethod: (AuthMethod) method
{
    static const NSString* kLogTag = @"show notification with image";

    extension::Logger::log(kLogTag, [NSString stringWithFormat:
        @"trying to show notification with an image, auth method is %d.", method]);

    if (!imageUrl || !imageUrl.length)
    {
        extension::Logger::log(kLogTag, @"no image url presented.");
        [self serviceExtensionTimeWillExpire];
        return;
    }

    NSURLSession* session = createSession(self, self.cloudSystemId, method);
    if (!session)
    {
        extension::Logger::log(kLogTag, @"can't create session.");
        [self serviceExtensionTimeWillExpire];
        return;
    }

    bool previewLoaded = false;
    const auto semaphore = dispatch_semaphore_create(0);
    const auto downloadTaskHandler =
        [self, method, &semaphore, &previewLoaded]
            (NSURL* tmpFileName, NSURLResponse* response, NSError* error)
        {
            const auto checkError =
                [&semaphore](bool error, const NSString* message = nil)
                {
                    if (!error)
                        return false;;

                    extension::Logger::log(kLogTag, message);

                    dispatch_semaphore_signal(semaphore);
                    return true;
                };

            if (checkError(!!error, @"error while loading image."))
                return;

            const long statusCode = statusCodeFromResponse(response);
            if (checkError(!isSuccessHttpStatus(statusCode), [NSString stringWithFormat:
                @"can't download image: HTTP error %ld, method %d", statusCode, method]))
            {
                return;
            }

            UNNotificationAttachment* attachment = makeAttachment(tmpFileName, kLogTag);
            if (checkError(!attachment))
                return;

            extension::Logger::log(kLogTag, @"image downloaded and attached.");
            previewLoaded = true;
            self.content.attachments = @[attachment];

            dispatch_semaphore_signal(semaphore);
        };

    NSURL* url = [NSURL URLWithString: imageUrl];
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL: url];

    const NSURLSessionDownloadTask* task = [session
        downloadTaskWithRequest: request
        completionHandler: downloadTaskHandler];

    [task resume];
    static const int64_t kTimeout = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::seconds(10)).count();
    if (dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, kTimeout)))
        [task cancel];

    if (!previewLoaded)
    {
        [self serviceExtensionTimeWillExpire];
        return;
    }

    extension::Logger::log(kLogTag, @"show custom notification.");
    self.handler(self.content);
}

- (void)URLSession:(NSURLSession*) session
    task: (NSURLSessionTask*) task
    didReceiveChallenge: (NSURLAuthenticationChallenge*) challenge
    completionHandler:
        (void (^)(NSURLSessionAuthChallengeDisposition, NSURLCredential*)) completionHandler
{
    static const NSString* kLogTag = @"digest authorization challenge";
    if (self.password.empty())
    {
        extension::Logger::log(kLogTag, @"can't load image because the password is empty.");
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, {});
        return;
    }

    if ([challenge previousFailureCount] > 0)
    {
        extension::Logger::log(kLogTag, @"can't load image.");
        completionHandler(NSURLSessionAuthChallengeCancelAuthenticationChallenge, {});
        return;
    }

    const auto credentials = [NSURLCredential
        credentialWithUser: [NSString stringWithUTF8String: self.user.c_str()]
        password: [NSString stringWithUTF8String: self.password.c_str()]
        persistence: NSURLCredentialPersistenceNone];

    [[challenge sender] useCredential: credentials forAuthenticationChallenge: challenge];
    completionHandler(NSURLSessionAuthChallengeUseCredential, credentials);
}

@end
