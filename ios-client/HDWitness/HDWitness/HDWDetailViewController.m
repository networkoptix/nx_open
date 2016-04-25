//
//  HDWDetailViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/21/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "Config.h"

#import "AFJSONRequestOperation.h"
#import "HDWHTTPRequestOperation.h"
#import "SVProgressHUD.h"
#import "UIImage+FX.h"

#import "NSString+Base64.h"
#import "NSString+MD5.h"
#import "NSString+OptixAdditions.h"

#import "HDWURLBuilder.h"
#import "HDWQueue.h"
#import "HDWModelEvent.h"
#import "HDWDetailViewController.h"
#import "HDWCollectionViewCell.h"
#import "HDWModelVisibilityVisitor.h"
#import "HDWModelVisibilityVisitor.h"
#import "HDWCompatibility.h"
#import "HDWAppDelegate.h"
#import "HDWApiConnectInfo.h"
#import "HDWProtocol.h"
#import "HDWQualityHelper.h"
#import "HDWECSConfig.h"
#import "HDWGlobalData.h"

#include "version.h"

#define THUMBNAIL_REFRESH_INTERVAL 60

enum State {
    State_Undefined,
    State_Initial,
    State_NotFound,
    State_NeedSendHA1,
    State_Connecting,
    State_Connected,
    
    State_IncompatibleEcsVersion,
    State_TooNewVersion,
    State_NetworkFailed,
    State_Disconnected
};

@interface HDWDetailViewController () {
    AFHTTPRequestOperation *_requestOperation;
    BOOL _presentedPopoverOnStart;
    
    HDWModelVisibilityVisitor *_visibilityVisitor;
    HDWECSModel *_visibleModel;
    
    id _offlineFilter;
    NSTimer *_thumbnailUpdateTimer;
    NSString *_disconnectReason;
    int _thumbnailRefreshIndex;
    
    UIAlertView *_disconnectedAlertView;
    UIAlertView *_goTo25AlertView;
}

@property(nonatomic) HDWECSModel *ecsModel;
@property(nonatomic) enum State state;
@end

@interface Header : UICollectionReusableView
@property (weak, nonatomic) IBOutlet UIImageView *image;
@property (weak, nonatomic) IBOutlet UILabel *label;
@property (weak, nonatomic) IBOutlet UILabel *summaryLabel;
@end

@implementation Header

- (void)loadFromServer:(HDWServerModel*)server {
    self.label.text = server.name;
    self.summaryLabel.text = [NSString stringWithFormat:L(@"Address: %@"), server.streamingBaseUrl.host];
    NSString *imageFileName = server.status.intValue == Status_Online ? @"server.png" : @"server_offline.png";
    self.image.image = [UIImage imageNamed:imageFileName];
}

@end

@implementation HDWDetailViewController {
}

#pragma mark - HDWCameraAccessor

- (HDWCameraModel *)cameraForUniqueId:(NSString *)uniqueId {
    return [_ecsModel findCameraByPhysicalId:uniqueId];
}

#pragma mark - UIViewController

- (void) prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([[segue identifier] isEqualToString:@"showVideo"]) {
        NSArray* selectedItems = [self.collectionView indexPathsForSelectedItems];
        NSIndexPath *indexPath = selectedItems[0];
        
        HDWCameraModel *cameraCopy = [_visibleModel cameraAtIndexPath: indexPath];
        HDWCameraModel *camera = [_ecsModel findCameraById:cameraCopy.cameraId];
        [[segue destinationViewController] setStreamDescriptors: [HDWQualityHelper matchStreamDescriptors:camera.streamDescriptors]];
        [[segue destinationViewController] useProtocol:_protocol cameraId:camera.physicalId andDelegate:self];
    }
}

- (int)currentThumbnailRefreshIndex {
    return (int)[[NSDate date] timeIntervalSince1970] * 2 / THUMBNAIL_REFRESH_INTERVAL;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    _collectionViewUpdatesDisabled = NO;
    _hideOfflineButtonItem.possibleTitles = [NSSet setWithObjects:L(@"Show Offline"), L(@"Hide Offline"), nil];
    _hideOfflineButtonItem.title = L(@"Hide Offline");
    _offlineFilter = nil;
    _thumbnailRefreshIndex = self.currentThumbnailRefreshIndex;
    
    _presentedPopoverOnStart = NO;
    [[FXImageView processingQueue] setMaxConcurrentOperationCount:3];
    [[FXImageView processedImageCache] removeAllObjects];
    
    [SVProgressHUD setBackgroundColor:[UIColor blackColor]];
    [SVProgressHUD setForegroundColor:[UIColor whiteColor]];
    
    _state = State_Undefined;
    
    if (_ecsConfig) {
        _state = State_Initial;
        [self performNextStep];
    }
}

- (void)viewDidUnload {
    self.hideOfflineButtonItem = nil;
    
    [super viewDidUnload];
}

- (void)dealloc {
    [self freeResources];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    
    [self updateView];
    [SVProgressHUD dismiss];
}

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];

    _state = State_Undefined;
    if (_requestOperation) {
        [_requestOperation cancel];
        _requestOperation = nil;
    }
    
    if (_thumbnailUpdateTimer) {
        [_thumbnailUpdateTimer invalidate];
        _thumbnailUpdateTimer = nil;
    }
}

- (BOOL)shouldAutorotate {
    return YES;
}


- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskAll;
}


- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    return UIInterfaceOrientationLandscapeLeft;
}


- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation {
    return YES;
    
}

- (void)updateView {
    if (_collectionViewUpdatesDisabled)
        return;
    
    _collectionViewUpdatesDisabled = YES;
    
    HDWECSModel *newVisibleModel = [_visibilityVisitor processFiltersForModel:_ecsModel];
    
    // Compare old and new visible models and generate events which should update old to new
    NSArray *pendingEventSets = [self compareVisibleModel:_visibleModel withModel:newVisibleModel];

    __block int updateCounter = 0;
    
    if (pendingEventSets.count == 0)
        return;
    
    for (NSArray *pendingEvents in pendingEventSets) {
        if (pendingEvents.count == 0)
            continue;

        updateCounter++;
        @try {
            [self.collectionView performBatchUpdates:^{
                NSUInteger size = pendingEvents.count;
                
                NSMutableArray* indexPaths = [NSMutableArray array];
                for (int n = 0; n < size; n++) {
                    HDWModelEvent *event = pendingEvents[n];
                    
                    NSIndexPath *indexPath = [event processModel:_visibleModel];
#ifdef DEBUG_COLLECTION_VIEW
                    NSLog(@"Processed model for indexPath: %@", indexPath);
#endif
                    if (indexPath && ![event isSkipViewUpdate]) {
                        [indexPaths addObject:indexPath];
                    } else {
                        [indexPaths addObject:nil];
                    }
                }
                
                for (int n = 0; n < size; n++) {
                    HDWModelEvent *event = pendingEvents[n];
                    
                    NSIndexPath *indexPath = [indexPaths objectAtIndex:n];
                    if (indexPath && ![event isSkipViewUpdate]) {
                        [event processView:self.collectionView atIndexPath:indexPath];
                    }
                }
            } completion:^(BOOL finished) {
                updateCounter--;
                
                if (updateCounter == 0) {
                    _collectionViewUpdatesDisabled = NO;
                    
                    dispatch_async(dispatch_get_main_queue(), ^{
                        [self updateView];
                    });
                }
            }];
        }
        @catch (NSException *exception) {
            [self.collectionView reloadData];
        }
    }
    
    if (updateCounter == 0)
        _collectionViewUpdatesDisabled = NO;
}

#pragma mark - HDWEventSource

- (void)eventSourceDidOpen:(HDWEventSource *)eventSource {
    
}

- (void)eventSource:(HDWEventSource *)eventSource didFailWithError:(NSError *)error {
    __weak typeof(self) weakSelf = self;
    
    dispatch_async(dispatch_get_main_queue(), ^(void){
        weakSelf.state = State_Initial;
        [weakSelf performNextStep];
    });
}

- (void)eventSource:(HDWEventSource *)eventSource didReceiveMessage:(Event *)event {
    if ([_protocol processMessage:event.data withECS:_ecsModel]) {
        [self updateView];
    }
}

#pragma mark - Managing the detail item

- (void)setEcsConfig:(id)newEcsConfig {
    if (_ecsConfig != newEcsConfig) {
        _ecsConfig = [newEcsConfig copy];
        _ecsConfig.login = [_ecsConfig.login lowercaseString];
        _ecsModel = [[HDWECSModel alloc] initWithECSConfig:_ecsConfig];
        _visibleModel = [[HDWECSModel alloc] init];
        NSArray *preFilters = @[[[HDWAuthorizedCameraFilter alloc] init], [[HDWDisabledCameraFilter alloc] init]];
        NSArray *postFilters = @[[[HDWNoCamerasServerFilter alloc] init]];
        
        _visibilityVisitor = [[HDWModelVisibilityVisitor alloc] initWithPre:preFilters andPostFilters:postFilters];
    }

    if (self.masterPopoverController != nil) {
        [self.masterPopoverController dismissPopoverAnimated:YES];
    }        
}

- (void) observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    HDWServerModel *server = (HDWServerModel *)object;
    
    if ([keyPath isEqualToString:@"streamingUrl"]) {
        NSUInteger serverIndex = [_visibleModel indexOfServerWithId:server.serverId];
        if (serverIndex != NSNotFound)
            [self.collectionView reloadSections:[NSIndexSet indexSetWithIndex:serverIndex]];
    }
}

- (void)setAuthCookies {
    if (![_protocol useBasicAuth])
        [HDWHTTPRequestOperation setAuthCookiesForConfig:self.ecsConfig];
}

- (void)connectRequestFinished: (id)data {
    id<HDWProtocol> protocol = nil;
    HDWApiConnectInfo *connectInfo = [HDWProtocolFactory parseConnectInfo:data andDetectProtocol:&protocol];
    if (!connectInfo) {
        _state = State_IncompatibleEcsVersion;
        [self performNextStep];
        return;
    }
    
    _protocol = protocol;

    [self setAuthCookies];
    
#ifdef DEBUG
    _state = State_Connected;
    [self performNextStep];
    return;
#endif
    
    NSArray *brandComponents = [connectInfo.brand componentsSeparatedByString:@"_"];
    NSArray *productNameComponents = [@QN_PRODUCT_NAME_SHORT componentsSeparatedByString:@"_"];
    
    if (connectInfo.brand && ![connectInfo.brand isEmpty] && ![brandComponents[0] isEqualToString:productNameComponents[0]]) {
        _state = State_IncompatibleEcsVersion;
        [self performNextStep];
        return;
    }
    
    HDWCompatibilityChecker *localChecker = [[HDWCompatibilityChecker alloc] initLocal];
    HDWCompatibilityChecker *remoteChecker = [[HDWCompatibilityChecker alloc] initWithItems:connectInfo.compatibilityItems];
    HDWCompatibilityChecker *checker;
    
    if (remoteChecker.itemCount > localChecker.itemCount) {
        checker = remoteChecker;
    } else {
        checker = localChecker;
    }
    
    NSString *currentVersion = [[NSBundle mainBundle] infoDictionary][@"CFBundleShortVersionString"];

    if (![checker isCompatibleComponent:@"iOSClient" ofVersion:currentVersion withComponent:@"ECS" ofVersion:connectInfo.version]
        || [connectInfo.version hasPrefix:@"1.5.0"]) {
        DDLogInfo(@"Versions are incompatible: our is %@, EC is %@", currentVersion, connectInfo.version);
        
        _state = State_IncompatibleEcsVersion;
        [self performNextStep];
        
        return;
    }
    
    NSComparisonResult v25Comparison = [connectInfo.version compare:@"2.5"];
    if ((v25Comparison == NSOrderedDescending || v25Comparison == NSOrderedSame) && SYSTEM_VERSION_GREATER_THAN_OR_EQUAL_TO(@"8.0")) {
        _state = State_TooNewVersion;
        [self performNextStep];
        
        return;
    }
    
    // 1.5.0 is not supported, we check it manually here as compatibility framework only checkes major and minor
    _state = State_Connected;
    [self performNextStep];
}

- (NSArray *)compareVisibleModel:(HDWECSModel *)oldModel withModel:(HDWECSModel *)newModel {
    NSMutableArray *serverAddEvents = [NSMutableArray array];
    NSMutableArray *serverRemoveEvents = [NSMutableArray array];
    NSMutableArray *serverChangeEvents = [NSMutableArray array];
    
    NSMutableArray *cameraAddEvents = [NSMutableArray array];
    NSMutableArray *cameraRemoveEvents = [NSMutableArray array];
    NSMutableArray *cameraChangeEvents = [NSMutableArray array];
    
    int m = (int)oldModel.servers.count - 1;
    int n = (int)newModel.servers.count - 1;
    
    for(;;) {
        if (m < 0 && n < 0) {
            break;
        } else if (m < 0 && n >= 0) {
            HDWServerModel *newServer = [newModel.servers valueAtIndex:n];
            [serverAddEvents addObject:[[HDWServerAddEvent alloc] initWithServer:newServer andSection:n]];
            
            HDWCameraModel *camera;
            int j = 0;
            NSEnumerator *valuesEnumerator = [newServer.cameras valuesEnumerator];
            while (camera = [valuesEnumerator nextObject]) {
                NSIndexPath *indexPath = [NSIndexPath indexPathForItem:j++ inSection:n];
                HDWModelEvent *event = [[HDWCameraAddEvent alloc] initWithCamera:camera  andIndexPath:indexPath];
                [cameraAddEvents addObject:event];
            }
            
            n--;
        } else if (m >= 0 && n < 0) {
            HDWServerModel *oldServer = [oldModel.servers valueAtIndex:m];
            
            HDWCameraModel *camera;
            NSEnumerator *valuesEnumerator = [oldServer.cameras valuesEnumerator];
            while (camera = [valuesEnumerator nextObject]) {
                HDWModelEvent *event = [[HDWCameraRemoveEvent alloc] initWithCameraId:camera.cameraId];
                // [event setSkipViewUpdate:YES];
                [cameraRemoveEvents addObject:event];
            }

            [serverRemoveEvents addObject:[[HDWServerRemoveEvent alloc] initWithServerId:oldServer.serverId]];

            m--;
        } else {
            HDWServerModel *oldServer = [oldModel.servers valueAtIndex:m];
            HDWServerModel *newServer = [newModel.servers valueAtIndex:n];
            
            if (![oldServer.guid isEqual:newServer.guid]) {
                NSComparisonResult comparisonResult = [oldServer compare:newServer];
                if (comparisonResult == NSOrderedAscending) {
                    [serverAddEvents addObject:[[HDWServerAddEvent alloc] initWithServer:newServer andSection:n]];
                    
                    HDWCameraModel *camera;
                    int j = 0;
                    NSEnumerator *valuesEnumerator = [newServer.cameras valuesEnumerator];
                    while (camera = [valuesEnumerator nextObject]) {
                        NSIndexPath *indexPath = [NSIndexPath indexPathForItem:j++ inSection:n];
                        HDWModelEvent *event = [[HDWCameraAddEvent alloc] initWithCamera:camera  andIndexPath:indexPath];
                        [cameraAddEvents addObject:event];
                    }
                    
                    n--;
                } else if (comparisonResult == NSOrderedDescending) {
                    HDWCameraModel *camera;
                    NSEnumerator *valuesEnumerator = [oldServer.cameras valuesEnumerator];
                    while (camera = [valuesEnumerator nextObject]) {
                        HDWModelEvent *event = [[HDWCameraRemoveEvent alloc] initWithCameraId:camera.cameraId];
                        // [event setSkipViewUpdate:YES];
                        [cameraRemoveEvents addObject:event];
                    }
                    
                    [serverRemoveEvents addObject:[[HDWServerRemoveEvent alloc] initWithServerId:oldServer.serverId]];
                    m--;
                }
            } else {
                if (![oldServer isEqual:newServer]) {
                    [serverChangeEvents addObject:[[HDWServerChangeEvent alloc] initWithServer:newServer]];
                }
                
                int i = (int)oldServer.cameras.count - 1;
                int j = (int)newServer.cameras.count - 1;
                
                for (;;) {
                    if (i < 0 && j < 0) {
                        break;
                    } else if (i < 0 && j >= 0) {
                        HDWCameraModel *newCamera = [newServer.cameras valueAtIndex:j];
                        NSIndexPath *indexPath = [NSIndexPath indexPathForItem:j inSection:n];
                        HDWCameraAddEvent *event = [[HDWCameraAddEvent alloc] initWithCamera:newCamera andIndexPath:indexPath];
                        
                        [cameraAddEvents addObject:event];
                        j--;
                    } else if (i >= 0 && j < 0) {
                        HDWCameraModel *oldCamera = [oldServer.cameras valueAtIndex:i];
                        HDWModelEvent *event = [[HDWCameraRemoveEvent alloc] initWithCameraId:oldCamera.cameraId];
                        
                        [cameraRemoveEvents addObject:event];
                        i--;
                    } else {
                        HDWCameraModel *oldCamera = [oldServer.cameras valueAtIndex:i];
                        HDWCameraModel *newCamera = [newServer.cameras valueAtIndex:j];
                        
                        if (![oldCamera.cameraId isEqual:newCamera.cameraId]) {
                            NSComparisonResult comparisonResult = [oldCamera compare:newCamera];
                            if (comparisonResult == NSOrderedAscending) {
                                NSIndexPath *indexPath = [NSIndexPath indexPathForItem:j inSection:n];
                                HDWModelEvent *event = [[HDWCameraAddEvent alloc] initWithCamera:newCamera  andIndexPath:indexPath];
                                [cameraAddEvents addObject:event];
                                j--;
                            } else if (comparisonResult == NSOrderedDescending) {
                                HDWModelEvent *event = [[HDWCameraRemoveEvent alloc] initWithCameraId:oldCamera.cameraId];
                                [cameraRemoveEvents addObject:event];
                                i--;
                            }
                        } else {
                            if (![oldCamera.name isEqualToString:newCamera.name] || ![oldCamera.status isEqualToNumber:newCamera.status] || oldCamera.server == nil)
                                [cameraChangeEvents addObject:[[HDWCameraChangeEvent alloc] initWithCamera:newCamera]];
                            
                            i--;
                            j--;
                        }
                    }
                }

                m--;
                n--;
            }
        }
    }
    
    
    NSMutableArray *allEvents = [NSMutableArray array];
    
    [allEvents addObjectsFromArray:cameraRemoveEvents];
    [allEvents addObjectsFromArray:serverRemoveEvents];

    [allEvents addObjectsFromArray:[serverAddEvents sortedArrayUsingComparator:^NSComparisonResult(HDWServerAddEvent *e1, HDWServerAddEvent *e2) {
        return e1.section < e2.section ? NSOrderedAscending : NSOrderedDescending;
    }]];
    
    [allEvents addObjectsFromArray:[cameraAddEvents sortedArrayUsingComparator:^NSComparisonResult(HDWCameraAddEvent *e1, HDWCameraAddEvent *e2) {
        return [e1.indexPath compare:e2.indexPath];
    }]];
    
    [allEvents addObjectsFromArray:serverChangeEvents];
    [allEvents addObjectsFromArray:cameraChangeEvents];
    
    return @[allEvents];
}

- (void)increaseProgress: (NSMutableArray*)arguments {
    NSNumber *progress = arguments[0];
    NSString *statusMessage = arguments[1];
    id requestOperation = arguments[2];
    
    if (requestOperation != _requestOperation || (!_requestOperation.isExecuting && !_requestOperation.isReady)) {
        return;
    }
    
    double progressValue = progress.doubleValue;
    [SVProgressHUD showProgress:progressValue status:statusMessage];
    
    progress = @(progressValue + 0.01);
    
    if (progress.doubleValue < 1)
        [self performSelector:@selector(increaseProgress:) withObject:@[progress, statusMessage, requestOperation] afterDelay:0.2];
}

- (void)setRealm:(NSString *)realm andNonce:(NSString *)nonce {
    _ecsConfig.realm = realm;
    
    uint64_t nonceTime;
    [[NSScanner scannerWithString:nonce] scanHexLongLong:&nonceTime];
    _ecsConfig.timeDiff = nonceTime / 1e+6 - [[NSDate date] timeIntervalSince1970];
}

- (void)requestEntityForPath:(NSString*)path withBareAssRealm:(NSString *)realm {
    // bareass realm is for 2.4, http only
    NSString *scheme = @"http";
    NSURL *baseUrl = [_ecsConfig urlForScheme:scheme];
    NSURLCredential *credential = [_ecsConfig credential];
    
    NSURL *resourceUrl = [[HDWURLBuilder instance] URLWithString:path relativeToURL:baseUrl];
    NSMutableURLRequest *resourceRequest = [NSMutableURLRequest requestWithURL:resourceUrl cachePolicy:NSURLRequestReloadIgnoringLocalAndRemoteCacheData timeoutInterval:20.0];
    NSString *ha1 = [[NSString stringWithFormat:@"%@:%@:%@", credential.user, realm, credential.password] MD5Digest];
    [resourceRequest addValue:ha1 forHTTPHeaderField:@"X-Nx-Digest"];
    [resourceRequest addValue:realm forHTTPHeaderField:@"X-Nx-Realm"];
    [resourceRequest addValue:@"x" forHTTPHeaderField:@"X-Nx-Crypt-Sha512"];
    [resourceRequest addValue:credential.user forHTTPHeaderField: @"X-Nx-User-Name"];
    
    [self requestEntityWithRequest:resourceRequest andCredential:credential];
}

- (void)requestEntityForPath:(NSString*)path {
    // First we try 2.3 and "http", next 2.2 and "https"
    NSString *scheme = self.state == State_Initial ? @"http" : @"https";
    NSURL *baseUrl = [_ecsConfig urlForScheme:scheme];
    NSURLCredential *credential = [_ecsConfig credential];
    
    NSURL *resourceUrl = [[HDWURLBuilder instance] URLWithString:path relativeToURL:baseUrl];
    NSMutableURLRequest *resourceRequest = [NSMutableURLRequest requestWithURL:resourceUrl cachePolicy:NSURLRequestReloadIgnoringLocalAndRemoteCacheData timeoutInterval:20.0];
    
    if ([scheme isEqualToString:@"https"]) {
        NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", credential.user, credential.password];
        [resourceRequest addValue:[NSString stringWithFormat:@"Basic %@", [basicAuthCredentials base64Encode]] forHTTPHeaderField: @"Authorization"];
    }
    
    [resourceRequest addValue:credential.user forHTTPHeaderField: @"X-Nx-User-Name"];

    [self requestEntityWithRequest:resourceRequest andCredential:credential];
}

- (void)requestEntityWithRequest:(NSURLRequest *)request andCredential:(NSURLCredential *)credential {
    __weak typeof(self) weakSelf = self;
    
    _requestOperation = [[HDWHTTPRequestOperation alloc] initWithRequest:request andCredential:credential];
    
    _requestOperation.allowsInvalidSSLCertificate = YES;

    NSHTTPCookieStorage *cookieStorage = [NSHTTPCookieStorage sharedHTTPCookieStorage];
    for (NSHTTPCookie *each in cookieStorage.cookies) {
        [cookieStorage deleteCookie:each];
    }

    _requestOperation.credential = credential;
    [_requestOperation setCompletionBlockWithSuccess:^(AFHTTPRequestOperation *operation, id responseObject) {
        if (weakSelf.ecsConfig.realm == nil)
            weakSelf.ecsConfig.realm = ((HDWHTTPRequestOperation *)operation).realm;

        [weakSelf connectRequestFinished:responseObject];
    } failure:^(AFHTTPRequestOperation *operation, NSError *error) {
        long statusCode = [[error userInfo][AFNetworkingOperationFailingURLResponseErrorKey] statusCode];
        
        // 0 is returned in situation when we're trying to connect to 2.2's https with http url
        if ((statusCode == 404 || statusCode == 0) && weakSelf.state == State_Initial) {
            weakSelf.state = State_NotFound;
            [weakSelf performNextStep];
            return;
        }
        
        if (error.code == NSURLErrorCancelled)
            return;
        
        
        NSString *ldapRealm = [[operation.response allHeaderFields] objectForKey:@"X-Nx-Realm"];
        if (weakSelf.state == State_Initial && statusCode == 401 && ldapRealm != nil && !ldapRealm.isEmpty) {
            weakSelf.ecsConfig.realm = ldapRealm;
            weakSelf.state = State_NeedSendHA1;
            [weakSelf performNextStep];
            return;
        }
        
        weakSelf.state = State_NetworkFailed;
        [weakSelf performNextStep];
    }];
    
    [self increaseProgress:[NSMutableArray arrayWithObjects:@0, L(@"Connecting..."), _requestOperation, nil]];
    [_requestOperation start];
}

- (NSString *)v22ConnectPath {
    return @"api/connect/?format=pb";
}

- (NSString *)v23ConnectPath {
    return [NSString stringWithFormat:@"ec2/connect?login=%@&format=json", _ecsConfig.login];
}

- (void)connectMessagesChannel {
    NSURL *baseUrl = [_protocol urlForConfig:_ecsConfig];
    NSURLCredential *credential = _ecsConfig.credential;
    
    NSURL *messageUrl = [[HDWURLBuilder instance] URLWithString:_protocol.eventsPath relativeToURL:baseUrl];
    NSMutableURLRequest *messageRequest = [NSMutableURLRequest requestWithURL:messageUrl cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:0];
    NSString *userAgent = [NSString stringWithFormat:@"%@ iOS Client/%@", @QN_PRODUCT_NAME_LONG, @QN_APPLICATION_VERSION];
    [messageRequest addValue:userAgent forHTTPHeaderField:@"User-Agent"];
    
    if ([_protocol useBasicAuth]) {
        NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", credential.user, credential.password];
        [messageRequest addValue:[NSString stringWithFormat:@"Basic %@", [basicAuthCredentials base64Encode]] forHTTPHeaderField: @"Authorization"];
    }
    
    self.messageSource = [[[_protocol eventSourceClass] alloc] initWithURLRequest:messageRequest credential:credential andDelegate:self];
    [self.messageSource open];
}

- (void)freeResources {
    [self.messageSource close];
    self.messageSource = nil;
    
    _ecsModel = nil;
    _ecsConfig = nil;
    _visibleModel = nil;
}

- (void)clearCredentialsCache {
    // reset the credentials cache...
    NSDictionary *credentialsDict = [[NSURLCredentialStorage sharedCredentialStorage] allCredentials];
    
    if ([credentialsDict count] > 0) {
        // the credentialsDict has NSURLProtectionSpace objs as keys and dicts of userName => NSURLCredential
        NSEnumerator *protectionSpaceEnumerator = [credentialsDict keyEnumerator];
        id urlProtectionSpace;
        
        // iterate over all NSURLProtectionSpaces
        while (urlProtectionSpace = [protectionSpaceEnumerator nextObject]) {
            NSEnumerator *userNameEnumerator = [[credentialsDict objectForKey:urlProtectionSpace] keyEnumerator];
            id userName;
            
            // iterate over all usernames for this protectionspace, which are the keys for the actual NSURLCredentials
            while (userName = [userNameEnumerator nextObject]) {
                NSURLCredential *cred = [[credentialsDict objectForKey:urlProtectionSpace] objectForKey:userName];
                [[NSURLCredentialStorage sharedCredentialStorage] removeCredential:cred forProtectionSpace:urlProtectionSpace];
            }
        }
    }
}

- (void)performNextStep {
    switch (_state) {
        case State_Initial: {
            self.title = _ecsConfig.description;
            _ecsConfig.realm = nil;
            [self clearCredentialsCache];
            [self requestEntityForPath:[self v23ConnectPath]];
            break;
        }
            
        case State_NeedSendHA1: {
            [self requestEntityForPath:[self v23ConnectPath] withBareAssRealm:_ecsConfig.realm];
            break;
        }
            
        case State_NotFound: {
            [self requestEntityForPath:[self v22ConnectPath]];
            break;
        }
        
        case State_Connected: {
            [SVProgressHUD dismiss];
            [self connectMessagesChannel];
            break;
        }
         
        case State_NetworkFailed: {
            [SVProgressHUD dismiss];
            
            _disconnectReason = L(@"Can't connect to the System.");
            _state = State_Disconnected;
            [self performNextStep];
            break;
        }
            
        case State_IncompatibleEcsVersion: {
            _disconnectReason = L(@"The System is incompatible with the app");
            _state = State_Disconnected;
            [self performNextStep];
            
            break;
        }
            
        case State_TooNewVersion: {
            _goTo25AlertView = [[UIAlertView alloc]
                                initWithTitle:L(@"Upgrade Now!")
                                message:L(@"You're connecting to a System running v2.5 or newer.\nDownload our new app for a better mobile viewing experience.")
                                delegate:self
                                cancelButtonTitle:L(@"Not now")
                                otherButtonTitles:L(@"Download")
                                , nil];
            [_goTo25AlertView show];
            
            break;
        }
            
        case State_Disconnected: {
            [self freeResources];
            [self updateView];
            
            _disconnectedAlertView = [[UIAlertView alloc] initWithTitle:L(@"Error") message:_disconnectReason delegate:self cancelButtonTitle:L(@"OK") otherButtonTitles:nil];
            [_disconnectedAlertView show];
            break;
        }
            
        default: {
            break;
        }
    }
}

- (void)setMasterVisibleFixed {
    if (![self.masterPopoverController isPopoverVisible]) {
        [self.masterPopoverController presentPopoverFromRect:CGRectMake(40, 40, 20, 20) inView:[[UIApplication sharedApplication] keyWindow] permittedArrowDirections:UIPopoverArrowDirectionAny animated:YES];
    }
}

- (void)toggleMasterVisibleFixed:(id)sender {
    if ([self.masterPopoverController isPopoverVisible]) {
        [self.masterPopoverController dismissPopoverAnimated:YES];
    } else {
        [self.masterPopoverController presentPopoverFromRect:CGRectMake(40, 40, 20, 20) inView:[[UIApplication sharedApplication] keyWindow] permittedArrowDirections:UIPopoverArrowDirectionAny animated:YES];
    }
}

- (void) viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        if (SYSTEM_VERSION_LESS_THAN_OR_EQUAL_TO(@"7.0"))
            [self.collectionView setContentInset:UIEdgeInsetsMake(44, 0, 0, 0)];
        
        UIBarButtonItem *menuButton = [[UIBarButtonItem alloc] initWithTitle:L(@"Systems") style:UIBarButtonItemStylePlain target:self action:@selector(toggleMasterVisibleFixed:)];
        
        self.navigationItem.leftBarButtonItem = menuButton;
        
        if (self.masterPopoverController && !_presentedPopoverOnStart) {
            _presentedPopoverOnStart = YES;
            
            //this fixes the no window issue in iOS 5.0
            [self toggleMasterVisibleFixed:self];
        }

    }
    
    [self.collectionView reloadData];
    _thumbnailUpdateTimer = [NSTimer scheduledTimerWithTimeInterval:THUMBNAIL_REFRESH_INTERVAL target:self selector:@selector(refreshThumbnails) userInfo:nil repeats:YES];
};

- (void)willMoveToParentViewController:(UIViewController *)parent {
    [self.messageSource close];
}

- (void)refreshThumbnails {
    _thumbnailRefreshIndex = self.currentThumbnailRefreshIndex;
//    [[FXImageView processedImageCache] removeAllObjects];
    [self.collectionView reloadItemsAtIndexPaths:[self.collectionView indexPathsForVisibleItems]];
//    [self.collectionView reloadData];
}

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView {
    [self.collectionViewLayout invalidateLayout];
#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"numberOfSectionsInCollectionView is %d", _visibleModel.count);
#endif
    return _visibleModel.count;
}

-(NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section {
    HDWServerModel* server = [_visibleModel serverAtIndex: section];
#ifdef DEBUG_COLLECTION_VIEW
    NSLog(@"numberOfItemsInSection %d is %d", section, server.cameras.count);
#endif
    return server.cameras.count;
}

- (void)initImageView:(FXImageView*)imageView {
    imageView.asynchronous = YES;
    imageView.reflectionScale = 0.5f;
    imageView.reflectionAlpha = 0.25f;
    imageView.reflectionGap = 10.0f;
    imageView.shadowOffset = CGSizeMake(0.0f, 2.0f);
    imageView.shadowBlur = 5.0f;
    imageView.cornerRadius = 10.0f;
}

- (UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    HDWCollectionViewCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"CVCELL" forIndexPath:indexPath];

    cell.selected = YES;

    HDWCameraModel *camera = [_visibleModel cameraAtIndexPath:indexPath];
    NSURL *thumbnailUrl = [_protocol thumbnailUrlForCamera:camera];
    
    NSString *cacheKey;
    UIImage *cachedImage;
    
    // try 20 previous refresh intervals
    for (int i = 0; i < 20 ; ++i) {
        cacheKey = [thumbnailUrl.absoluteString stringByAppendingFormat:@"_%d", _thumbnailRefreshIndex - i];
        cachedImage = [FXImageView.processedImageCache objectForKey:cacheKey];

        if (cachedImage)
            break;
    }
    
    cacheKey = [thumbnailUrl.absoluteString stringByAppendingFormat:@"_%d", _thumbnailRefreshIndex];
    
    cell.imageView.processedImage = cachedImage;
    
    [self initImageView:cell.imageView];
    
    cell.labelView.text = camera.name;
    
    switch (camera.calculatedStatus.intValue) {
        case Status_Offline: {
            cell.imageView.cacheKey = @"camera_offline";
            [cell.imageView setImageWithContentsOfFile:@"camera_offline.png"];
            break;
        }
            
        case Status_Unauthorized: {
            cell.imageView.cacheKey = @"camera_unauthorized";
            [cell.imageView setImageWithContentsOfFile:@"camera_unauthorized.png"];
            break;
        }
            
        case Status_Online:
        case Status_Recording: {
            if (!cell.imageView.processedImage)
                cell.imageView.processedImage =
                    [[UIImage imageNamed:@"camera.png"] imageCroppedAndScaledToSize:cell.imageView.bounds.size
                                                 contentMode:UIViewContentModeScaleAspectFit padToFit:NO];
            
            
            if (thumbnailUrl) {
                cell.imageView.cacheKey = cacheKey;
#ifdef DEBUG_URLS
                NSLog(@"%@", thumbnailUrl.absoluteString);
#endif
                [self setAuthCookies];
                
                [cell.imageView setImageWithContentsOfURL:thumbnailUrl withCredential:_ecsConfig.credential useBasicAuth:[_protocol useBasicAuth]];
            } else {
                cell.imageView.cacheKey = @"camera_online";
            }
            break;
        }
        default: {
            cell.imageView.cacheKey = @"camera_offline";
            [cell.imageView setImageWithContentsOfFile:@"camera_offline.png"];
            NSLog(@"Camera %@ has unknown status: %@", camera.physicalId, camera.calculatedStatus);
        }
    }

    return cell;
}

- (UICollectionReusableView *)collectionView:(UICollectionView *)collectionView viewForSupplementaryElementOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)indexPath {
    
    if([kind isEqualToString:UICollectionElementKindSectionHeader]) {
        Header *header = [collectionView dequeueReusableSupplementaryViewOfKind:UICollectionElementKindSectionHeader withReuseIdentifier:@"headerTitle" forIndexPath:indexPath];
        HDWServerModel *server = [_visibleModel serverAtIndex:indexPath.section];
        [header loadFromServer:server];
        
        //modify your header
        return header;
    }
    
    return nil;
}

- (void)collectionView:(UICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
    HDWCameraModel *camera = [_visibleModel cameraAtIndexPath: indexPath];
    
    if (camera.calculatedStatus.intValue == Status_Unauthorized) {
        [[[UIAlertView alloc] initWithTitle:L(@"Error") message:L(@"Invalid Camera Credentials") delegate:nil cancelButtonTitle:L(@"OK") otherButtonTitles:nil] show];
        return;
    }
        
    if (camera.streamDescriptors.count == 0) {
        [[[UIAlertView alloc] initWithTitle:L(@"Error") message:L(@"There is no available resolution for this camera") delegate:nil cancelButtonTitle:L(@"OK") otherButtonTitles:nil] show];
        return;
    }
    
    [self performSegueWithIdentifier:@"showVideo" sender:[collectionView cellForItemAtIndexPath:indexPath]];
}

#pragma mark - Alert view

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex {
    if (alertView == _goTo25AlertView) {
        if (buttonIndex == 1) {
            NSString *urlString = [NSString stringWithFormat:@"%@%@",
                                   @"http://itunes.apple.com/app/",
                                   @QN_IOS_NEW_APP_APPSTORE_ID];
            
            [[UIApplication sharedApplication] openURL:[NSURL URLWithString:urlString]];
        }
        
        _state = State_Connected;
        [self performNextStep];
        
        return;
    }

    self.title = @"";
    [self.navigationController popToRootViewControllerAnimated:YES];
    [self setMasterVisibleFixed];
}

#pragma mark - Split view

- (void)splitViewController:(UISplitViewController *)splitController willHideViewController:(UIViewController *)viewController withBarButtonItem:(UIBarButtonItem *)barButtonItem forPopoverController:(UIPopoverController *)popoverController {
    barButtonItem.title = L(@"Systems");
    [self.navigationItem setLeftBarButtonItem:barButtonItem animated:YES];
    self.masterPopoverController = popoverController;
}

- (void)splitViewController:(UISplitViewController *)splitController willShowViewController:(UIViewController *)viewController invalidatingBarButtonItem:(UIBarButtonItem *)barButtonItem {
    // Called when the view is shown again in the split view, invalidating the button and popover controller.
    [self.navigationItem setLeftBarButtonItem:nil animated:YES];
    self.masterPopoverController = nil;
}

- (BOOL)splitViewController:(UISplitViewController*)svc
   shouldHideViewController:(UIViewController *)vc
              inOrientation:(UIInterfaceOrientation)orientation
{
    return YES;
}

#pragma mark - Actions

- (IBAction)onHideOffline:(UIBarButtonItem *)sender {
    if (_offlineFilter) {
        [_visibilityVisitor removeUserFilter:_offlineFilter];
        _offlineFilter = nil;
    } else {
        _offlineFilter = [_visibilityVisitor addUserFilter:[[HDWOfflineCameraFilter alloc] init]];
    }
    
    [self updateView];
    self.hideOfflineButtonItem.title = _offlineFilter ? L(@"Show Offline") : L(@"Hide Offline");
}
@end
