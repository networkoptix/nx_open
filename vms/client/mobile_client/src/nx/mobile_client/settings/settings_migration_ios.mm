#include "settings_migration.h"

#import <Foundation/Foundation.h>

#include <version.h>

@interface HDWECSConfig : NSObject

@property(nonatomic,retain) NSString *name;
@property(nonatomic,retain) NSString *host;
@property(nonatomic,retain) NSString *port;
@property(nonatomic,retain) NSString *login;
@property(nonatomic,retain) NSString *password;

- (BOOL)isValid;

@end

@implementation HDWECSConfig

- (BOOL)isValid
{
    // Only host is required for import
    return [_host length] > 0;
}

- (instancetype)initWithCoder:(NSCoder *)decoder
{
    if ((self = [super init]))
    {
        self.name = [decoder decodeObjectForKey:@"name"];
        self.host = [decoder decodeObjectForKey:@"host"];
        self.port = [decoder decodeObjectForKey:@"port"];
        self.login = [decoder decodeObjectForKey:@"login"];
        self.password = [decoder decodeObjectForKey:@"password"];
    }
    return self;
}

@end

namespace nx {
namespace mobile_client {
namespace settings {

QnMigratedSessionList getMigratedSessions(bool *success)
{
    if (success)
        *success = false;

    QnMigratedSessionList result;

    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@QN_IOS_SHARED_GROUP_ID];
    NSData *encodedObject = [defaults objectForKey:@"ecsconfigs"];
    if (!encodedObject)
        return result;

    NSMutableArray *objects = (NSMutableArray*)[NSKeyedUnarchiver unarchiveObjectWithData: encodedObject];

    for (NSUInteger i = 0; i < objects.count; ++i)
    {
        HDWECSConfig *config = static_cast<HDWECSConfig *>(objects[i]);
        if (![config isValid])
            continue;

        QnMigratedSession session;
        session.title = QString::fromNSString(config.name);
        session.host = QString::fromNSString(config.host);
        session.port = QString::fromNSString(config.port).toInt();
        session.login = QString::fromNSString(config.login);
        session.password = QString::fromNSString(config.password);

        result.append(session);
    }

    if (success)
        *success = true;

    return result;
}

} // namespace settings
} // namespace mobile_client
} // namespace nx
