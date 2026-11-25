// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_service.h"

#include <MobileCoreServices/MobileCoreServices.h>
#include <UIKit/UIKit.h>
#include <UserNotifications/UserNotifications.h>

#include <chrono>
#include <memory>
#include <mutex>

#include <nx/vms/client/mobile/push_notification/details/push_ipc_data.h>
#include <nx/vms/client/mobile/push_notification/details/push_notification_storage.h>

#include "utils/helpers.h"
#include "utils/ios_secure_storage.h"
#include "utils/logger.h"

using PushIpcData = nx::vms::client::mobile::details::PushIpcData;
using IosSecureStorage = nx::vms::client::mobile::details::IosSecureStorage;
using PushNotification = nx::vms::client::mobile::PushNotification;
using PushNotificationStorage = nx::vms::client::mobile::PushNotificationStorage;

namespace {

static const auto kAccessErrorText = @"No access to this information.";
static const int kUnauthorizedHttpError = 401;

using namespace extension::helpers;

enum class ImageLoadResult
{
    loaded,
    unauthorized,
    failed
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

PushIpcData::TokenInfo requestSystemAccessToken(
    const std::string& user,
    const std::string& cloudRefreshToken,
    const std::string& cloudSystemId)
{
    static const NSString* kLogTag = @"access token request";

    extension::Logger::log(kLogTag, @"started.");

    PushIpcData::TokenInfo result;
    if (user.empty() || cloudRefreshToken.empty() || cloudSystemId.empty())
    {
        extension::Logger::log(kLogTag, @"invalid parameters.");
        return result;
    }

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

NSURLSession* createSession(const std::string& accessToken)
{
    NSURLSessionConfiguration* config = [NSURLSessionConfiguration defaultSessionConfiguration];

    NSString* authString = [NSString stringWithFormat: @"Bearer %s", accessToken.c_str()];

    [config setHTTPAdditionalHeaders: @{@"Authorization": authString}];

    return [NSURLSession sessionWithConfiguration: config delegate: nil delegateQueue: nil];
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

@interface NotificationService ()
@property(nonatomic) ContentHanlder handler;
@property(nonatomic) UNMutableNotificationContent* content;
@property(nonatomic) std::string user;
@property(nonatomic) std::string cloudRefreshToken;
@property(nonatomic) std::string cloudSystemId;
@property(nonatomic) NSString* imageUrl;
- (bool) initialize: (NSString*) targets;
- (void) showNotification;
- (ImageLoadResult) attachImage;
- (ImageLoadResult) downloadImage: (const std::string&) accessToken;
- (void) saveNotificationImage: (NSURL*) sourceUrl;
- (void) saveNotification;
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
    self.imageUrl = data[@"imageUrl"];

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
        [self showNotification];
    else //< Since we can't prevent notification popup in iOS we just show it with restricted data.
        [self serviceExtensionTimeWillExpire];
}

- (void)serviceExtensionTimeWillExpire
{
    extension::Logger::log(@"show standard notification");
    [self saveNotification];
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

    std::string user, cloudRefreshToken;
    if (!PushIpcData::load(user, cloudRefreshToken))
    {
        extension::Logger::log(kLogTag,
            @"can't load current keychain user data, discarding notification");
        return false;
    }

    if (user.empty() || cloudRefreshToken.empty())
    {
        extension::Logger::log(kLogTag, @"not enough credentials data, discarding notification");
        return false;
    }

    self.user = user;
    self.cloudRefreshToken = cloudRefreshToken;

    const auto searchString = [NSString stringWithFormat: @"\"%s\"", self.user.c_str()];
    const bool isInTargets = [[targets lowercaseString] containsString: searchString];
    if (!isInTargets)
    {
        extension::Logger::log(kLogTag,
            @"current user is not found in targets, discarding notificiation");
    }

    return isInTargets;
}

- (void)showNotification
{
    static const NSString* kLogTag = @"show notification with image";

    if (!self.imageUrl || !self.imageUrl.length)
    {
        extension::Logger::log(kLogTag, @"no image url presented.");
        [self serviceExtensionTimeWillExpire];
        return;
    }

    if ([self attachImage] != ImageLoadResult::loaded)
    {
        [self serviceExtensionTimeWillExpire];
        return;
    }

    extension::Logger::log(kLogTag, @"show custom notification.");
    [self saveNotification];
    self.handler(self.content);
}

- (ImageLoadResult)attachImage
{
    // Use the saved token without checking it first. A new token is issued only if the server
    // rejects the saved one.
    const auto savedToken = PushIpcData::accessToken(self.cloudSystemId, self.user);
    if (!savedToken.accessToken.empty())
    {
        const auto result = [self downloadImage: savedToken.accessToken];
        if (result != ImageLoadResult::unauthorized)
            return result;

        PushIpcData::removeAccessToken(self.cloudSystemId, savedToken.accessToken);
    }

    const auto newToken =
        requestSystemAccessToken(self.user, self.cloudRefreshToken, self.cloudSystemId);
    if (!newToken.accessToken.empty())
    {
        const auto result = [self downloadImage: newToken.accessToken];
        if (result != ImageLoadResult::unauthorized)
        {
            PushIpcData::setAccessToken(
                self.cloudSystemId, self.user, newToken.accessToken, newToken.expiresAt);
            return result;
        }
    }

    return ImageLoadResult::failed;
}

- (ImageLoadResult)downloadImage: (const std::string&) accessToken
{
    static const NSString* kLogTag = @"download image";

    extension::Logger::log(kLogTag, @"trying to load an image.");

    NSURLSession* session = createSession(accessToken);

    // The handler can be called after the timeout, when this method has already returned and the
    // content may be already shown. So it uses shared state and does nothing after the timeout.
    struct DownloadState
    {
        std::mutex mutex;
        bool finished = false;
        ImageLoadResult result = ImageLoadResult::failed;
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    };
    const auto state = std::make_shared<DownloadState>();

    const auto downloadTaskHandler =
        [self, state](NSURL* tmpFileName, NSURLResponse* response, NSError* error)
        {
            const std::lock_guard<std::mutex> lock(state->mutex);
            if (state->finished)
                return;

            const auto checkError =
                [&state](bool error, const NSString* message = nil)
                {
                    if (!error)
                        return false;;

                    extension::Logger::log(kLogTag, message);

                    dispatch_semaphore_signal(state->semaphore);
                    return true;
                };

            if (checkError(!!error, @"error while loading image."))
                return;

            const long statusCode = statusCodeFromResponse(response);
            if (statusCode == kUnauthorizedHttpError)
                state->result = ImageLoadResult::unauthorized;

            if (checkError(!isSuccessHttpStatus(statusCode), [NSString stringWithFormat:
                @"can't download image: HTTP error %ld", statusCode]))
            {
                return;
            }

            [self saveNotificationImage: tmpFileName]; //< Save the image before creating UNNotificationAttachment.
            UNNotificationAttachment* attachment = makeAttachment(tmpFileName, kLogTag);
            if (checkError(!attachment))
                return;

            extension::Logger::log(kLogTag, @"image downloaded and attached.");
            state->result = ImageLoadResult::loaded;
            self.content.attachments = @[attachment];

            dispatch_semaphore_signal(state->semaphore);
        };

    NSURL* url = [NSURL URLWithString: self.imageUrl];
    NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL: url];

    const NSURLSessionDownloadTask* task = [session
        downloadTaskWithRequest: request
        completionHandler: downloadTaskHandler];

    [task resume];
    static const int64_t kTimeout = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::seconds(10)).count();
    if (dispatch_semaphore_wait(state->semaphore, dispatch_time(DISPATCH_TIME_NOW, kTimeout)))
        [task cancel];

    const std::lock_guard<std::mutex> lock(state->mutex);
    state->finished = true;
    return state->result;
}

- (void)saveNotificationImage: (NSURL*) sourceUrl
{
    static const NSString* kLogTag = @"save notification image";

    NSData* data = [NSData dataWithContentsOfURL: sourceUrl];
    if (!data)
    {
        extension::Logger::log(kLogTag, @"Failed to load image.");
        return;
    }

    const auto ptr = static_cast<const std::byte*>([data bytes]);
    std::vector<std::byte> image{ptr, ptr + data.length};

    auto secureStorage = std::make_shared<IosSecureStorage>();
    PushNotificationStorage notificationStorage{secureStorage};
    notificationStorage.saveImage([self.imageUrl UTF8String], image);
}

- (void)saveNotification
{
    extension::Logger::log(@"Save notification");

    NSString* url = [self.content.userInfo objectForKey: @"url"];

    auto secureStorage = std::make_shared<IosSecureStorage>();
    PushNotificationStorage notificationStorage{secureStorage};
    const std::string id = notificationStorage.addUserNotification(
        self.user,
        [self.content.title UTF8String],
        [self.content.body UTF8String],
        url ? [url UTF8String] : "",
        self.cloudSystemId,
        [self.imageUrl UTF8String]);

    NSURLComponents* newUrl = [NSURLComponents componentsWithString: url];

    NSMutableArray<NSURLQueryItem*>* newItems =
        newUrl.queryItems ? [newUrl.queryItems mutableCopy] : [NSMutableArray array];

    NSURLQueryItem* idItem = [NSURLQueryItem
        queryItemWithName: @"push_notification_id"
        value: [NSString stringWithUTF8String: id.c_str()]];

    [newItems addObject: idItem];
    newUrl.queryItems = newItems;
    NSMutableDictionary* userInfo = [self.content.userInfo mutableCopy];
    userInfo[@"url"] = newUrl.string;
    self.content.userInfo = userInfo;
}

@end
