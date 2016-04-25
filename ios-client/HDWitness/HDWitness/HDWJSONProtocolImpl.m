//
//  HDWJSONProtocol.m
//  HDWitness
//
//  Created by Ivan Vigasin on 7/21/14.
//  Copyright (c) 2014 Ivan Vigasin. All rights reserved.
//

#import "HDWUUID.h"
#import "HDWJSONProtocolImpl.h"
#import "HDWEventSource.h"
#import "HDWCameraModel.h"
#import "HDWECSConfig.h"
#import "HDWApiConnectInfo.h"
#import "HDWURLBuilder.h"

#import "AFHTTPRequestOperation.h"
#import "NSString+BasicAuthUtils.h"
#import "NSString+Base64.h"
#import "NSString+MD5.h"

#define DEBUG_URLS

enum HDWTransacionCommand {
    NotDefined                  = 0,
    
    /* System */
    tranSyncRequest             = 1,    /*< ApiSyncRequestData */
    tranSyncResponse            = 2,    /*< QnTranStateResponse */
    lockRequest                 = 3,    /*< ApiLockData */
    lockResponse                = 4,    /*< ApiLockData */
    unlockRequest               = 5,    /*< ApiLockData */
    peerAliveInfo               = 6,    /*< ApiPeerAliveData */
    tranSyncDone                = 7,    /*< ApiTranSyncDoneData */
    
    /* Connection */
    testConnection              = 100,  /*< ApiLoginData */
    connect                     = 101,  /*< ApiLoginData */
    
    /* Common resource */
    saveResource                = 200,  /*< ApiResourceData */
    removeResource              = 201,  /*< ApiIdData */
    setResourceStatus           = 202,  /*< ApiResourceStatusData */
    getResourceParams           = 203,  /*< ApiResourceParamDataList */
    setResourceParams           = 204,  /*< ApiResourceParamWithRefDataList */
    getResourceTypes            = 205,  /*< ApiResourceTypeDataList*/
    getFullInfo                 = 206,  /*< ApiFullInfoData */
    setPanicMode                = 207,  /*< ApiPanicModeData */
    setResourceParam            = 208,   /*< ApiResourceParamWithRefData */
    removeResourceParam         = 209,   /*< ApiResourceParamWithRefData */
    removeResourceParams        = 210,   /*< ApiResourceParamWithRefDataList */
    getStatusList               = 211,  /*< ApiResourceStatusDataList */
    removeResources             = 212,  /*< ApiIdDataList */
    
    /* Camera resource */
    getCameras                  = 300,  /*< ApiCameraDataList */
    saveCamera                  = 301,  /*< ApiCameraData */
    saveCameras                 = 302,  /*< ApiCameraDataList */
    removeCamera                = 303,  /*< ApiIdData */
    getCameraHistoryItems       = 304,  /*< ApiCameraServerItemDataList */
    addCameraHistoryItem        = 305,  /*< ApiCameraServerItemData */
    addCameraBookmarkTags       = 306,  /*< ApiCameraBookmarkTagDataList */
    getCameraBookmarkTags       = 307,  /*< ApiCameraBookmarkTagDataList */
    removeCameraBookmarkTags    = 308,
    removeCameraHistoryItem     = 309,  /*< ApiCameraServerItemData */
    saveCameraUserAttributes    = 310,  /*< ApiCameraAttributesData */
    saveCameraUserAttributesList= 311,  /*< ApiCameraAttributesDataList */
    getCameraUserAttributes     = 312,  /*< ApiCameraAttributesDataList */
    getCamerasEx                = 313,  /*< ApiCameraDataExList */
    
    
    /* MediaServer resource */
    getMediaServers             = 400,  /*< ApiMediaServerDataList */
    saveMediaServer             = 401,  /*< ApiMediaServerData */
    removeMediaServer           = 402,  /*< ApiIdData */
    saveServerUserAttributes    = 403,  /*< QnMediaServerUserAttributesList */
    saveServerUserAttributesList= 404,  /*< QnMediaServerUserAttributesList */
    getServerUserAttributes     = 405,  /*< ApiIdData, QnMediaServerUserAttributesList */
    removeServerUserAttributes  = 406,  /*< ApiIdData */
    saveStorage                 = 407,  /*< ApiStorageData */
    saveStorages                = 408,  /*< ApiStorageDataList */
    removeStorage               = 409,  /*< ApiIdData */
    removeStorages              = 410,  /*< QList<ApiIdData> */
    getMediaServersEx           = 411,  /*< ApiMediaServerDataExList */
    getStorages                 = 412,  /*< ApiStorageDataList */
    
    /* User resource */
    getUsers                    = 500,  /*< ApiUserDataList */
    saveUser                    = 501,  /*< ApiUserData */
    removeUser                  = 502,  /*< ApiIdData */
    
    /* Layout resource */
    getLayouts                  = 600,  /*< ApiLayoutDataList */
    saveLayout                  = 601,  /*< ApiLayoutDataList */
    saveLayouts                 = 602,  /*< ApiLayoutData */
    removeLayout                = 603,  /*< ApiIdData */
    
    /* Videowall resource */
    getVideowalls               = 700,  /*< ApiVideowallDataList */
    saveVideowall               = 701,  /*< ApiVideowallData */
    removeVideowall             = 702,  /*< ApiIdData */
    videowallControl            = 703,  /*< ApiVideowallControlMessageData */
    
    /* Business rules */
    getBusinessRules            = 800,  /*< ApiBusinessRuleDataList */
    saveBusinessRule            = 801,  /*< ApiBusinessRuleData */
    removeBusinessRule          = 802,  /*< ApiIdData */
    resetBusinessRules          = 803,  /*< ApiResetBusinessRuleData */
    broadcastBusinessAction     = 804,  /*< ApiBusinessActionData */
    execBusinessAction          = 805,  /*< ApiBusinessActionData */
    
    /* Stored files */
    listDirectory               = 900,  /*< ApiStoredDirContents */
    getStoredFile               = 901,  /*< ApiStoredFileData */
    addStoredFile               = 902,  /*< ApiStoredFileData */
    updateStoredFile            = 903,  /*< ApiStoredFileData */
    removeStoredFile            = 904,  /*< ApiStoredFilePath */
    
    /* Licenses */
    getLicenses                 = 1000, /*< ApiLicenseDataList */
    addLicense                  = 1001, /*< ApiLicenseData */
    addLicenses                 = 1002, /*< ApiLicenseDataList */
    removeLicense               = 1003, /*< ApiLicenseData */
    
    /* Email */
    testEmailSettings           = 1100, /*< ApiEmailSettingsData */
    sendEmail                   = 1101, /*< ApiEmailData */
    
    /* Auto-updates */
    uploadUpdate                = 1200, /*< ApiUpdateUploadData */
    uploadUpdateResponce        = 1201, /*< ApiUpdateUploadResponceData */
    installUpdate               = 1202, /*< ApiUpdateInstallData  */
    
    /* Module information */
    moduleInfo                  = 1301, /*< ApiModuleData */
    moduleInfoList              = 1302, /*< ApiModuleDataList */
    
    /* Discovery */
    discoverPeer                = 1401, /*< ApiDiscoveryData */
    addDiscoveryInformation     = 1402, /*< ApiDiscoveryData*/
    removeDiscoveryInformation  = 1403, /*< ApiDiscoveryData*/
    
    /* Misc */
    forcePrimaryTimeServer      = 2001,  /*< ApiIdData */
    broadcastPeerSystemTime     = 2002,  /*< ApiPeerSystemTimeData*/
    getCurrentTime              = 2003,  /*< ApiTimeData */
    changeSystemName            = 2004,  /*< ApiSystemNameData */
    getKnownPeersSystemTime     = 2005,  /*< ApiPeerSystemTimeDataList */
    markLicenseOverflow         = 2006,  /*< ApiLicenseOverflowData */
    
    //getHelp                     = 9003,  /*< ApiHelpGroupDataList */
    runtimeInfoChanged          = 9004,  /*< ApiRuntimeData */
    dumpDatabase                = 9005,  /*< ApiDatabaseDumpData */
    restoreDatabase             = 9006,  /*< ApiDatabaseDumpData */
    updatePersistentSequence              = 9009,  /*< ApiUpdateSequenceData*/
    
    maxTransactionValue         = 65535
};

@implementation HDWJSONProtocolImpl {
    HDWApiConnectInfo *_connectInfo;
    NSMutableDictionary *_resourceStatuses;
}

- (id)init:(HDWApiConnectInfo *)connectInfo {
    self = [super init]; 
    if (self) {
        _connectInfo = connectInfo;
        _resourceStatuses = [NSMutableDictionary dictionary];
    }
    
    return self;
}

- (BOOL)useBasicAuth {
    return [_connectInfo.version compare:@"2.4"] == NSOrderedAscending;
}

- (NSString *)systemTimePath {
    return @"ec2/getCurrentTime?format=json";
}

- (NSURL *)urlForConfig:(HDWECSConfig *)config {
    return [config urlForScheme:@"http"];
}

+ (HDWApiConnectInfo *)parseConnectInfo:(NSData *)data {
    HDWApiConnectInfo *connectInfo = [[HDWApiConnectInfo alloc] init];
    
    NSError *error;
    NSDictionary *connectInfoDict = [NSJSONSerialization JSONObjectWithData:data options:nil error:&error];
    connectInfo.version = connectInfoDict[@"version"];
    connectInfo.brand = connectInfoDict[@"brand"];
    connectInfo.serverGuid = [[HDWUUID alloc] initWithUUIDString:connectInfoDict[@"ecsGuid"]];
    return connectInfo;
}

- (NSString *)eventsPath {
    return [NSString stringWithFormat:@"/ec2/events?isMobile=true&format=json"];
}

- (Class)eventSourceClass {
    return [HDWEventSource class];
}

- (int)statusToInt:(NSString *)status {
    if ([status isEqualToString:@"Offline"]) {
        return 0;
    } else if ([status isEqualToString:@"Unauthorized"]) {
        return 1;
    } else if ([status isEqualToString:@"Online"]) {
        return 2;
    } else if ([status isEqualToString:@"Recording"]) {
        return 3;
    }
    
    return -1;
}

- (NSDictionary *)jsonServerToDict:(NSDictionary *)jsonServer {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    
    dict[@"id"] = [HDWResourceId resourceIdWithString:jsonServer[@"id"]];
    dict[@"name"] = jsonServer[@"name"];
    dict[@"guid"] = [HDWResourceId resourceIdWithString:jsonServer[@"id"]];
    dict[@"status"] = @([self statusToInt:jsonServer[@"status"]]);
    dict[@"netAddrList"] = jsonServer[@"networkAddresses"];
    dict[@"apiUrl"] = jsonServer[@"apiUrl"];
    
    return dict;
}

- (NSDictionary *)jsonCameraToDict:(NSDictionary *)jsonCamera {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    
    dict[@"id"] = [HDWResourceId resourceIdWithString:jsonCamera[@"id"]];
    dict[@"parentId"] = [HDWResourceId resourceIdWithString:jsonCamera[@"parentId"]];
    dict[@"name"] = jsonCamera[@"name"];
    if ([jsonCamera.allKeys indexOfObject:@"status"] != NSNotFound) {
        dict[@"status"] = @([self statusToInt:jsonCamera[@"status"]]);
    }

    if ([jsonCamera.allKeys indexOfObject:@"secondaryStreamQuality"] != NSNotFound) {
        dict[@"secondaryStreamQuality"] = jsonCamera[@"secondaryStreamQuality"];
    }
    
    dict[@"disabled"] = [NSNumber numberWithInt:NO];
    dict[@"physicalId"] = jsonCamera[@"physicalId"];
    for (NSDictionary *param in jsonCamera[@"addParams"]) {
        NSString *paramName = param[@"name"];
        NSString *paramValue = param[@"value"];
        
        if ([paramName isEqualToString:@"mediaStreams"]) {
            NSData *data = [paramValue dataUsingEncoding:NSUTF8StringEncoding];
            NSError *e = nil;
            NSDictionary *jsonDict = [NSJSONSerialization JSONObjectWithData:data options:nil error: &e];
            if (!jsonDict) {
                NSLog(@"Error parsing JSON: %@", e);
                break;
            }

            if ( [jsonDict.allKeys indexOfObject:@"streams"] != NSNotFound) {
                dict[@"streams"] = jsonDict[@"streams"];
            }
        } else if ([paramName isEqualToString:@"rotation"]) {
            if (paramValue.length > 0)
                dict[@"angle"] = [NSNumber numberWithInteger:[paramValue integerValue]];
        } else if ([paramName isEqualToString:@"overrideAr"]) {
            if (paramValue.length > 0)
                dict[@"aspectRatio"] = [NSNumber numberWithDouble:[paramValue doubleValue]];
        }
    }
    
    return dict;
}

- (NSDictionary *)jsonUserToDict:(NSDictionary *)jsonUser {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    
    dict[@"id"] = [HDWResourceId resourceIdWithString:jsonUser[@"id"]];
    dict[@"name"] = jsonUser[@"name"];
    dict[@"isAdmin"] = jsonUser[@"isAdmin"];
    dict[@"rights"] = @([jsonUser[@"permissions"] longLongValue]);
    
    return dict;
}

- (NSDictionary *)jsonLayoutToDict:(NSDictionary *)jsonLayout {
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    
    dict[@"id"] = [HDWResourceId resourceIdWithString:jsonLayout[@"id"]];
    dict[@"name"] = jsonLayout[@"name"];
    dict[@"parentId"] = [HDWResourceId resourceIdWithString:jsonLayout[@"parentId"]];
    dict[@"items"] = [NSMutableArray array];
    
    for (NSDictionary * data in jsonLayout[@"items"]) {
        [dict[@"items"] addObject:@{@"resourceId": [HDWResourceId resourceIdWithString:data[@"resourceId"]]}];
    }
    
    return dict;
}

- (void)replaceServer:(NSDictionary *)jsonServer onECS:(HDWECSModel *)ecs {
    HDWServerModel* server = [[HDWServerModel alloc] initWithDict:[self jsonServerToDict:jsonServer] andECS:ecs];
    if (server.status.intValue == -1) {
        HDWServerModel* existingServer = [ecs findServerById:server.serverId];
        NSNumber *status = [_resourceStatuses objectForKey:server.serverId];
        if (status != nil)
            server.status = status;
        else if (existingServer != nil)
            server.status = existingServer.status;
        else
            server.status = [NSNumber numberWithInt:Status_Offline];
    }
    
    [ecs addOrUpdateServer:server];
}

- (void)mergeCamera:(NSDictionary *)jsonCamera onECS:(HDWECSModel *)ecs {
    if ([jsonCamera[@"model"] isEqualToString:@"virtual desktop camera"])
        return;
    
    NSDictionary *cameraDict = [self jsonCameraToDict:jsonCamera];
    
    /*
     This line was added to hide IO/module. Unfortunately it also hide unauthorized cameras. Commenting for now..
     
    if ([cameraDict.allKeys containsObject:@"streams"])
     */
    [ecs mergeCameraFromDict:cameraDict];
}

- (void)addCameraServerItem:(NSDictionary *)item toECS:(HDWECSModel *)ecs {
    HDWCameraModel *camera = [ecs findCameraByPhysicalId:item[@"cameraUniqueId"]];
    if (camera) {
        HDWResourceId *serverId = [HDWResourceId resourceIdWithString:item[@"serverGuid"]];
        HDWServerModel *server = [ecs findServerById:serverId];
        [camera addCameraServerItem:[[HDWCameraServerItem alloc] initWithServer:server andTimestamp:[item[@"timestamp"] longLongValue]]];
    }
}

- (BOOL)processMessage:(NSData *)messageData withECS:(HDWECSModel *)ecs {
    NSError *error;
    NSDictionary *messageDict = [NSJSONSerialization JSONObjectWithData:messageData options:nil error:&error];
  
    NSDictionary *tran = messageDict[@"tran"];
    id params = tran[@"params"];
    
    enum HDWTransacionCommand command = [tran[@"command"] intValue];

#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"Event: %d", command);
#endif
    
    switch (command) {
        case runtimeInfoChanged:
            break;

        // temporarily commented as app craches
        case saveMediaServer: {
            [self replaceServer:params onECS:ecs];
            break;
        }
            
           // temporarily commented as app craches
        case saveServerUserAttributes: {
            NSString *serverId = params[@"serverID"];
            NSString *serverName = params[@"serverName"];
            
            if (serverId && serverName && serverName.length > 0) {
                HDWServerModel *server = [ecs findServerById:[HDWResourceId resourceIdWithString:serverId]];
                if (server && ![server.name isEqualToString:serverName]) {
                    server.name = serverName;
                    return YES;
                }
            }
            
            return NO;
        }
            
        case saveCamera: {
            [self mergeCamera:params onECS:ecs];
            break;
        }
        
        case saveLayout: {
            HDWUserModel *user = [ecs findUserById:[HDWResourceId resourceIdWithString:params[@"parentId"]]];
            if (user) {
                HDWLayoutModel *layout = [[HDWLayoutModel alloc] initWithDict:[self jsonLayoutToDict:params] andUser:user findInCameras:ecs.camerasById];
                
                [user addOrReplaceLayout:layout];
            }

            break;
        }
            

        case getMediaServersEx: {
            for (NSDictionary *jsonServer in params) {
                [self replaceServer:jsonServer onECS:ecs];
            }
            
            break;
        }
            
        case getCamerasEx:
        case saveCameras: {
            for (NSDictionary *jsonCamera in params) {
                [self mergeCamera:jsonCamera onECS:ecs];
            }
            
            break;
        }
            
        case getUsers: {
            for (NSDictionary *jsonUser in params) {
                HDWUserModel *user = [[HDWUserModel alloc] initWithDict:[self jsonUserToDict:jsonUser] andECS:ecs];
                [ecs addUser:user];
            }
            
            break;
        }
            
        case getLayouts:
        case saveLayouts: {
            for (NSDictionary *jsonLayout in params) {
                HDWResourceId *userId = [HDWResourceId resourceIdWithString:jsonLayout[@"parentId"]];

                HDWUserModel *user = [ecs findUserById:userId];
                if (user) {
                    HDWLayoutModel *layout = [[HDWLayoutModel alloc] initWithDict:[self jsonLayoutToDict:jsonLayout] andUser:user findInCameras:ecs.camerasById];
                 
                    [user addOrReplaceLayout:layout];
                }
            }
            break;
        }
            
        case removeLayout: {
            HDWResourceId *layoutId = [HDWResourceId resourceIdWithString:params[@"id"]];
            
            HDWUserModel *user;
            NSEnumerator *valuesEnumerator = [ecs.users valuesEnumerator];
            while (user = [valuesEnumerator nextObject]) {
                [user.layouts removeObjectForKey:layoutId];
            }
            
            break;
        }
            
        case removeCamera: {
            [ecs removeCameraById:[HDWResourceId resourceIdWithString:params[@"id"]]];
            break;
        }
            
        case setResourceStatus: {
            NSNumber *status = [NSNumber numberWithInt:[self statusToInt:params[@"status"]]];
            HDWResourceId *resourceId = [HDWResourceId resourceIdWithString:params[@"id"]];
            
            [_resourceStatuses setObject:status forKey:resourceId];
            
            HDWServerModel *server = [ecs findServerById:resourceId];
            if (server) {
                if (![server.status isEqualToNumber:status]) {
                    [server setStatus:status];
                    return YES;
                }
            } else {
                HDWCameraModel *camera = [ecs findCameraById:resourceId];
                if (camera && ![camera.status isEqualToNumber:status]) {
                    [camera setStatus:status];
                    return YES;
                }
            }
            
            return NO;
        }
            
        case saveResource: {
            return NO;
        }
            
        case getCameraHistoryItems: {
            for (NSDictionary *item in params) {
                [self addCameraServerItem:item toECS:ecs];
            }
            break;
        }
            
        case addCameraHistoryItem: {
            [self addCameraServerItem:params toECS:ecs];
            break;
        }
            
        case saveCameraUserAttributes: {
            NSDictionary *jsonCamera = (NSDictionary *)params;
            NSString *name = jsonCamera[@"cameraName"];
            NSString *cameraId = jsonCamera[@"cameraID"];
            
            if (name) {
                HDWCameraModel *camera = [ecs findCameraById:[HDWResourceId resourceIdWithString:cameraId]];
                if (camera && ![camera.name isEqualToString:name]) {
                    camera.name = name;
                    return YES;
                }
            }
            
            return NO;
        }
            
        case peerAliveInfo:
        case broadcastBusinessAction:
        case broadcastPeerSystemTime:
        case lockRequest:
        case lockResponse: {
            return NO;
        }
        
        default:
//            NSLog(@"%d", command);
            return NO;
    }
    
    return YES;
}

- (NSURL *)timeUrlForServer:(HDWServerModel *)server {
    NSString *proxyPrefix = @"";
    
    if (![_connectInfo.serverGuid isEqual:[server.guid toUUID]]) {
        proxyPrefix = [NSString stringWithFormat:@"/proxy/http/%@", [server.guid toString]];
    }
    
    NSString *path = @"api/gettime";
    NSURL *baseUrl = [self urlForConfig:server.ecs.config];
    NSURL *url = [[HDWURLBuilder instance] URLWithString:[path stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding] relativeToURL:baseUrl];
#ifdef DEBUG_URLS
    NSLog(@"Chunks at Server %@: %@", server.serverId, url.absoluteString);
#endif
    return url;
}

- (NSURL *)chunksUrlForCamera:(HDWCameraModel *)camera atServer:(HDWServerModel *)server {
    NSString *proxyPrefix = @"";
    
    if (![_connectInfo.serverGuid isEqual:[server.guid toUUID]]) {
        proxyPrefix = [NSString stringWithFormat:@"/proxy/http/%@", [server.guid toString]];
    }

    NSString *path = [NSString stringWithFormat:@"%@/api/RecordedTimePeriods?physicalId=%@&startTime=0&endTime=4000000000000&periodsType=0&detail=60000&format=bin", proxyPrefix, camera.physicalId];
    NSURL *baseUrl = [self urlForConfig:server.ecs.config];
    NSURL *url = [[HDWURLBuilder instance] URLWithString:[path stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding] relativeToURL:baseUrl];
#ifdef DEBUG_URLS
    NSLog(@"Chunks at Server %@: %@", server.serverId, url.absoluteString);
#endif
    return url;
}

- (NSURL*)videoUrlForCamera:(HDWCameraModel *)camera date:(NSTimeInterval)date streamDescriptor:(HDWStreamDescriptor *)streamDescriptor andQuality:(NSUInteger)quality {
    NSString *position, *path, *proxyPrefix = @"";
    
    HDWServerModel *server = date ? [camera onlinServerForFutureDate:date withChunks:camera.serverChunks] : camera.server;
    
    if (![_connectInfo.serverGuid isEqual:[server.guid toUUID]]) {
        proxyPrefix = [NSString stringWithFormat:@"/proxy/http/%@", [server.guid toString]];
    }
    
    if (streamDescriptor.transport == VT_HLS) {
        if (date) {
            int64_t mksecs = date * 1000000ll;
            position = [NSString stringWithFormat:@"%lld", mksecs];
            path = [NSString stringWithFormat:@"%@/hls/%@.m3u8?%@&startTimestamp=%@", proxyPrefix, camera.physicalId, streamDescriptor.requestParameter, position];
        } else {
            path = [NSString stringWithFormat:@"%@/hls/%@.m3u8?%@&live", proxyPrefix, camera.physicalId, streamDescriptor.requestParameter];
        }
    } else {
        if (date) {
            int64_t msecs = date * 1000ll;
            position = [NSString stringWithFormat:@"%lld", msecs];
        } else {
            position = @"now";
        }
        
        path = [NSString stringWithFormat:@"%@/media/%@.mpjpeg?resolution=%dp&qmin=%d&qmax=%d&pos=%@&sfd=", proxyPrefix, camera.physicalId, streamDescriptor.resolution.height, quality, quality, position];
    }
    
    NSURL *baseUrl = [self urlForConfig:camera.server.ecs.config];
    NSURL *url = [[HDWURLBuilder instance]
                  URLWithString:[path stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding] relativeToURL:baseUrl];
#ifdef DEBUG_URLS
    NSLog(@"VideoUrl: %@", url.absoluteString);
#endif
    return url;
}

- (NSURL *)liveUrlForCamera:(HDWCameraModel *)camera andStreamDescriptor:(HDWStreamDescriptor *)streamDescriptor {
    return [self videoUrlForCamera:camera date:0 streamDescriptor:streamDescriptor andQuality:10];
}

- (NSURL *)archiveUrlForCamera:(HDWCameraModel *)camera date:(NSTimeInterval)date andStreamDescriptor:(HDWStreamDescriptor *)streamDescriptor {
    return [self videoUrlForCamera:camera date:date streamDescriptor:streamDescriptor andQuality:10];
}

- (NSURL *)thumbnailUrlForCamera:(HDWCameraModel *)camera; {
    NSString *proxyPrefix = @"";
    if (![_connectInfo.serverGuid isEqual:[camera.server.guid toUUID]]) {
        proxyPrefix = [NSString stringWithFormat:@"/proxy/http/%@", [camera.server.guid toString]];
    }
    
    NSString *path = [NSString stringWithFormat:@"%@/api/image?physicalId=%@&time=latest&height=360&format=png", proxyPrefix, camera.physicalId];
    NSURL *baseUrl = [self urlForConfig:camera.server.ecs.config];
    return [[HDWURLBuilder instance] URLWithString:[path stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding] relativeToURL:baseUrl];
}

@end
