//
//  HDWDetailViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/21/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "NSString+Base64.h"

#import "HDWDetailViewController.h"
#import "HDWCollectionViewCell.h"

#import "AFJSONRequestOperation.h"
#import "AFHTTPClient.h"
#import "SVProgressHUD.h"

#import "connectinfo.pb.h"

enum MessageType {
    MessageType_Initial                 = 0,
    MessageType_Ping                    = 1,
    
    MessageType_ResourceChange          = 2,
    MessageType_ResourceDelete          = 3,
    MessageType_ResourceStatusChange    = 4,
    MessageType_ResourceDisabledChange  = 5,
    
    MessageType_License                 = 6,
    MessageType_CameraServerItem        = 7,
    
    MessageType_BusinessRuleChange      = 8,
    MessageType_BusinessRuleDelete      = 9,
    
    MessageType_BroadcastBusinessAction = 10
};

enum State {
    State_Initial,
    State_Connecting,
    State_Connected,
    State_RequestingResources,
    State_GotResources,
    
    State_IncompatibleEcsVersion,
    State_NetworkFailed
};

@interface HDWDetailViewController () {
    NSURL *_baseUrl;
    AFHTTPRequestOperation *_requestOperation;
    enum State _state;
    SVProgressHUD *_progressHUD;
}

@property (strong, nonatomic) UIPopoverController *masterPopoverController;

@end

@interface Header : PSUICollectionReusableView
@property (weak, nonatomic) IBOutlet UIImageView *image;
@property (weak, nonatomic) IBOutlet UILabel *label;
@property (weak, nonatomic) IBOutlet UILabel *summaryLabel;
@end

@implementation Header

- (void)loadFromServer:(HDWServerModel*)server {
    self.label.text = server.name;
    self.summaryLabel.text = [NSString stringWithFormat:@"Address: %@", server.streamingUrl.host];
    if (server.status.intValue == Status_Online) {
        self.image.image = [UIImage imageNamed:@"server.png"];
    } else {
        self.image.image = [UIImage imageNamed:@"server_offline.png"];
    }
}

@end

@implementation HDWDetailViewController

- (BOOL)shouldAutorotate {
    return YES;
}


- (NSUInteger)supportedInterfaceOrientations {
    return UIInterfaceOrientationMaskLandscape;
}


- (UIInterfaceOrientation)preferredInterfaceOrientationForPresentation {
    return UIInterfaceOrientationLandscapeLeft;
}


- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)orientation {
    if (orientation == UIInterfaceOrientationPortraitUpsideDown) {
        return NO;
    }
    
    return YES;
    
}

- (void)webSocketDidOpen:(SRWebSocket *)webSocket {
    NSLog(@"webSocketDidOpen");
    _state = State_NetworkFailed;
    [self performNextStep];
}

- (void)webSocket:(SRWebSocket *)webSocket didFailWithError:(NSError *)error {
     NSLog(@"didFailWithError");
    _state = State_NetworkFailed;
    [self performNextStep];
}

- (void)webSocket:(SRWebSocket *)webSocket didCloseWithCode:(NSInteger)code reason:(NSString *)reason wasClean:(BOOL)wasClean {
     NSLog(@"didCloseWithCode");
    _state = State_NetworkFailed;
    [self performNextStep];
}

- (void)handleDisableChange:(HDWCameraModel*)camera withDisabled:(BOOL)disabled newCameras:(NSMutableArray*)newCameras andRemovedCameras:(NSMutableArray*)removedCameras {
    if (disabled) {
        NSIndexPath *indexPath = [_ecsModel indexPathOfCameraWithId:camera.cameraId andServerId:camera.server.serverId];
        [camera setDisabled:YES];
        [removedCameras addObject:indexPath];
    } else {
        [camera setDisabled:NO];
        NSIndexPath *indexPath = [_ecsModel indexPathOfCameraWithId:camera.cameraId andServerId:camera.server.serverId];
        [newCameras addObject:indexPath];
    }
}

- (void)webSocket:(SRWebSocket *)webSocket didReceiveMessage:(id)rawMessage {
    NSString *messageString = (NSString*)rawMessage;
    NSData *messageData = [messageString dataUsingEncoding:NSUTF8StringEncoding];
    
    NSError* error;
    NSDictionary *message = [NSJSONSerialization JSONObjectWithData:messageData options:kNilOptions error:&error];
    
    int messageType = [message[@"type"] intValue];
    NSString *objectName = message[@"objectName"];

    NSLog(@"Message: %d, Object:%@", messageType, objectName);
    
    NSMutableIndexSet *newServers = [NSMutableIndexSet indexSet];
    NSMutableArray *newCameras = [NSMutableArray array];
    
    NSMutableArray *removedCameras = [NSMutableArray array];
    NSMutableIndexSet *removedServers = [NSMutableIndexSet indexSet];

    [self.collectionView performBatchUpdates:^{

        if (messageType == MessageType_ResourceChange) {
            NSDictionary *resource = message[@"resource"];
            
            // resource changed
            if ([objectName isEqual: @"Server"]) {
                HDWServerModel* server = [[HDWServerModel alloc] initWithDict:resource andECS:_ecsModel];
                
                NSUInteger index = [_ecsModel addOrUpdateServer:server];
                if (index != NSNotFound) {
                    [newServers addIndex:index];
                }
            } else if ([objectName isEqual: @"Camera"]) {
                NSNumber *cameraId = resource[@"id"];
                NSNumber *serverId = resource[@"parentId"];
                
                HDWCameraModel* existing = [_ecsModel findCameraById:cameraId atServer:serverId];
                HDWServerModel *server = [_ecsModel findServerById:serverId];
                
                HDWCameraModel* camera = [[HDWCameraModel alloc] initWithDict:resource andServer:server];

                NSLog(@"Before: %d", server.enabledCameras.count);
                NSIndexPath* indexPath = [_ecsModel addOrReplaceCamera:camera];
                if (indexPath) { // Camera is added
                    [newCameras addObject:indexPath];
                } else if (camera.disabled != existing.disabled) {
                    camera.disabled = existing.disabled;
                    [self handleDisableChange:camera withDisabled:!camera.disabled newCameras:newCameras andRemovedCameras:removedCameras];
                }
                NSLog(@"After: %d", server.enabledCameras.count);
            }
        } else if (messageType == MessageType_ResourceDelete) {
            NSNumber *resourceId = message[@"resourceId"];
            NSNumber *parentId = message[@"parentId"];
            
            if ([_ecsModel findServerById:resourceId]) {
                NSUInteger serverIndex = [_ecsModel indexOfServerWithId:resourceId];
                
                if (serverIndex != NSNotFound) {
                    [removedServers addIndex:serverIndex];
                    [_ecsModel removeServerById:resourceId];
                }
            } else if ([_ecsModel findCameraById:resourceId atServer:parentId]) {
                NSIndexPath *indexPath = [_ecsModel indexPathOfCameraWithId:resourceId andServerId:parentId];
                if (indexPath) {
                    [removedCameras addObject:indexPath];
                    [_ecsModel removeCameraById:resourceId andServerId:parentId];
                }
            }
        } else if (messageType == MessageType_ResourceStatusChange) {
            if ([message objectForKey:@"parentId"]) {
                HDWCameraModel *camera = [_ecsModel findCameraById:message[@"resourceId"] atServer:message[@"parentId"]];
                [camera setStatus:message[@"status"]];
            } else {
                HDWServerModel *server = [_ecsModel findServerById:message[@"resourceId"]];
                [server setStatus:message[@"status"]];
            }
        } else if (messageType == MessageType_ResourceDisabledChange) {
            NSNumber *cameraId = message[@"resourceId"];
            NSNumber *serverId = message[@"parentId"];
            
            HDWCameraModel *camera = [_ecsModel findCameraById:cameraId atServer:serverId];
            int disabled = ((NSString*)message[@"disabled"]).intValue;
            if (camera.disabled != disabled)
                [self handleDisableChange:camera withDisabled:disabled newCameras:newCameras andRemovedCameras:removedCameras];
        }

        if (removedCameras.count != 0) {
            [self.collectionView deleteItemsAtIndexPaths:removedCameras];
        }
        if (removedServers.count != 0) {
            [self.collectionView deleteSections:removedServers];
        }
        
        if (newServers.count != 0) {
            NSLog(@"Inserting servers");
            [self.collectionView insertSections:newServers];
        }
        
        if (newCameras.count != 0) {
            NSLog(@"Inserting cameras");
            [self.collectionView insertItemsAtIndexPaths:newCameras];
        }
        
        [self.collectionView reloadData];
//        [self.collectionView reloadItemsAtIndexPaths:indexPaths];
    } completion:nil];
}

#pragma mark - Managing the detail item

- (void)setEcsConfig:(id)newEcsConfig {
    if (_ecsConfig != newEcsConfig) {
        _ecsConfig = newEcsConfig;
        _ecsModel = [[HDWECSModel alloc] initWithECSConfig:newEcsConfig];
    }

    if (self.masterPopoverController != nil) {
        [self.masterPopoverController dismissPopoverAnimated:YES];
    }        
}

- (void)_loadResourcesFromJSON:(id)JSON toModel:(HDWECSModel*)model {
    NSMutableDictionary* serversDict = [[NSMutableDictionary alloc] init];
    
    NSArray *jsonServers = JSON[@"servers"];
    for (NSDictionary *jsonServer in jsonServers) {
        HDWServerModel *server = [[HDWServerModel alloc] initWithDict:jsonServer andECS:_ecsModel];
        
        [serversDict setObject:server forKey:server.serverId];
    }
    
    NSArray *jsonCameras = JSON[@"cameras"];
    for (NSDictionary *jsonCamera in jsonCameras) {
        HDWServerModel *server = [serversDict objectForKey: jsonCamera[@"parentId"]];
        HDWCameraModel *camera = [[HDWCameraModel alloc] initWithDict:jsonCamera andServer:server];
        
        [server addOrReplaceCamera:camera];
    }
    
    [model addServers: [serversDict allValues]];
}

- (void)connectRequestFinished: (id)data {
    ConnectInfo *connectInfo = [ConnectInfo parseFromData:data];
    if ([connectInfo.version hasPrefix:@"1.5."]) {
        _state = State_Connected;
    } else {
        _state = State_IncompatibleEcsVersion;
    }
    
    [self performNextStep];
}

- (void)resourceRequestFinished: (id)JSON {
    [self _loadResourcesFromJSON:JSON toModel:_ecsModel];
    [self.collectionView reloadData];

    _state = State_GotResources;
    [self performNextStep];
}

- (void)alertView:(UIAlertView *)alertView didDismissWithButtonIndex:(NSInteger)buttonIndex {
    [self.navigationController popToRootViewControllerAnimated:YES];
}

- (void)increaseProgress: (NSMutableArray*)arguments {
    NSNumber *progress = arguments[0];
    NSString *statusMessage = arguments[1];
    id requestOperation = arguments[2];
    
    if (requestOperation != _requestOperation || (!_requestOperation.isExecuting && !_requestOperation.isReady)) {
//        [SVProgressHUD dismiss];
        return;
    }
    
    double progressValue = progress.doubleValue;
    [SVProgressHUD showProgress:progressValue status:statusMessage];
    
    progress = [NSNumber numberWithDouble:progressValue + 0.02];
    
    if (progress.doubleValue < 1)
        [self performSelector:@selector(increaseProgress:) withObject:@[progress, statusMessage, requestOperation] afterDelay:0.2];
}


- (void)requestJSONEntityForResourceKind:(NSString*)resourceKind success:(SEL)onSuccess statusMessage:(NSString*)statusMessage errorMessage:(NSString*)errorMessage {
    NSString *path = [NSString stringWithFormat:@"api/%@/?format=json", resourceKind];
    NSURL *resourceUrl = [NSURL URLWithString:path relativeToURL:_baseUrl];
    NSURLRequest *resourceRequest = [NSURLRequest requestWithURL:resourceUrl cachePolicy:NSURLCacheStorageNotAllowed timeoutInterval:10.0];
    
    _requestOperation = [AFJSONRequestOperation JSONRequestOperationWithRequest:resourceRequest success:^(NSURLRequest *request, NSHTTPURLResponse *response, id JSON) {
        [SVProgressHUD dismiss];
        [self performSelector:onSuccess withObject:JSON];
    } failure:^(NSURLRequest *request, NSHTTPURLResponse *response, NSError *error, id JSON) {
        [SVProgressHUD dismiss];
        if (error.code == NSURLErrorCancelled)
            return;
        
        _state = State_NetworkFailed;
        [[[UIAlertView alloc] initWithTitle:@"Error" message:errorMessage delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil] show];
    }];
    
    [self increaseProgress:[NSMutableArray arrayWithObjects:@0, statusMessage, _requestOperation, nil]];
    [_requestOperation start];
}

- (void)requestPB2EntityForResourceKind:(NSString*)resourceKind success:(SEL)onSuccess statusMessage:(NSString*)statusMessage errorMessage:(NSString*)errorMessage {
    NSString *path = [NSString stringWithFormat:@"api/%@/?format=pb", resourceKind];
    NSURL *resourceUrl = [NSURL URLWithString:path relativeToURL:_baseUrl];
    NSURLRequest *resourceRequest = [NSURLRequest requestWithURL:resourceUrl cachePolicy:NSURLCacheStorageNotAllowed timeoutInterval:10.0];
    
    _requestOperation = [[AFHTTPRequestOperation alloc] initWithRequest:resourceRequest];
    [_requestOperation setCompletionBlockWithSuccess:^(AFHTTPRequestOperation *operation, id responseObject) {
        [SVProgressHUD dismiss];
        [self performSelector:onSuccess withObject:responseObject];
    } failure:^(AFHTTPRequestOperation *operation, NSError *error) {
        [SVProgressHUD dismiss];
        if (error.code == NSURLErrorCancelled)
            return;
        
        _state = State_NetworkFailed;
        [[[UIAlertView alloc] initWithTitle:@"Error" message:errorMessage delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil] show];
    }];
    
    
    [self increaseProgress:[NSMutableArray arrayWithObjects:@0, statusMessage, _requestOperation, nil]];
    [_requestOperation start];
}

- (void)connectMessagesChannel {
    NSURL *messageUrl = [NSURL URLWithString:@"/websocket/" relativeToURL:_baseUrl];
    NSMutableURLRequest *messageRequest = [NSMutableURLRequest requestWithURL:messageUrl cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:0];
    
    NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", messageUrl.user, messageUrl.password];
    [messageRequest addValue:[NSString stringWithFormat:@"Basic %@", [basicAuthCredentials base64Encode]] forHTTPHeaderField: @"Authorization"];
    
    _socket = [[SRWebSocket alloc] initWithURLRequest:messageRequest];
    [_socket setDelegate:self];
    [_socket open];
}

- (void)performNextStep {
    switch (_state) {
        case State_NetworkFailed: {
            break;
        }
            
        case State_Initial: {
            [self requestPB2EntityForResourceKind:@"connect" success:@selector(connectRequestFinished:) statusMessage:@"Connecting to Enterprise Controller..."errorMessage:@"Can't connect to the System."];
            break;
        }
            
        case State_Connected: {
            [self requestJSONEntityForResourceKind:@"resource" success:@selector(resourceRequestFinished:) statusMessage:@"Requesting Enterprise Controller resources..." errorMessage:@"Can't get resources."];
            break;
        }
         
        case State_GotResources: {
            [self connectMessagesChannel];
            break;
        }
            
        case State_IncompatibleEcsVersion: {
            [[[UIAlertView alloc] initWithTitle:@"Error" message:@"This ECS is incompatible with the app" delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil] show];
            break;
        }
            
        default:
            break;
    }
}

- (void)configureView {
    if (!_ecsConfig)
        return;
    
    
    _progressHUD = [[SVProgressHUD alloc] init];

    NSString* urlString = [NSString stringWithFormat:@"https://%@:%@@%@:%@/", _ecsConfig.login, _ecsConfig.password, _ecsConfig.host, _ecsConfig.port];
    _baseUrl = [NSURL URLWithString:urlString];
    
    _state = State_Initial;
    [self performNextStep];
    
    self.detailDescriptionLabel.text = [_ecsConfig description];
    
//    if (self.ecsConfig) {
//        self.detailDescriptionLabel.text = [_ecsConfig description];
//    }
}

- (void)viewDidLoad {
    [super viewDidLoad];

    [[FXImageView processingQueue] setMaxConcurrentOperationCount:100];
    
	// Do any additional setup after loading the view, typically from a nib.
    [self configureView];
    
    self.videoViewController = (HDWVideoViewController *)[[self.splitViewController.viewControllers lastObject] topViewController];
}

- (void) viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        UIBarButtonItem *menuButton = [[UIBarButtonItem alloc] initWithTitle:@"Systems" style:UIBarButtonItemStylePlain target:self.splitViewController action:@selector(toggleMasterVisible:)];
        
        self.navigationItem.leftBarButtonItem = menuButton;
    }
    
//    self.splitViewController toggleM
//    self.navigationController.navigationBar
};

- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    
    if (_requestOperation) {
        [_requestOperation cancel];
    }
}

- (void)willMoveToParentViewController:(UIViewController *)parent {
    [_socket setDelegate:nil];
    [_socket close];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

-(NSInteger)numberOfSectionsInCollectionView:(PSUICollectionView *)collectionView {
    return _ecsModel.count;
}

-(NSInteger)collectionView:(PSUICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section {
    HDWServerModel* server = [_ecsModel serverAtIndex: section];
    
    return server.enabledCameras.count;
}

-(PSUICollectionViewCell *)collectionView:(PSUICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath {
    HDWCollectionViewCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"MyCell" forIndexPath:indexPath];

    cell.imageView.asynchronous = YES;
    cell.imageView.reflectionScale = 0.5f;
    cell.imageView.reflectionAlpha = 0.25f;
    cell.imageView.reflectionGap = 10.0f;
    cell.imageView.shadowOffset = CGSizeMake(0.0f, 2.0f);
    cell.imageView.shadowBlur = 5.0f;
    cell.imageView.cornerRadius = 10.0f;

    HDWCameraModel *camera = [_ecsModel cameraAtIndexPath:indexPath];

    switch (camera.status.intValue) {
        case Status_Offline: {
            [cell.imageView setImageWithContentsOfFile:@"camera_offline.png"];
            break;
        }
            
        case Status_Unauthorized: {
            [cell.imageView setImageWithContentsOfFile:@"camera_unauthorized.png"];
            break;
        }
            
        case Status_Online:
        case Status_Recording: {
            [cell.imageView setImageWithContentsOfFile:@"camera.png"];
            break;
        }	
    }
    
    if (camera.status.intValue == Status_Online || camera.status.intValue == Status_Recording) {
        NSLog(@"Thumbnail URL: %@", [camera.thumbnailUrl absoluteString]);
        [cell.imageView setImageWithContentsOfURL:camera.thumbnailUrl];
    }
    
    cell.labelView.text = camera.name;
    
    return cell;
}

- (PSUICollectionReusableView *)collectionView:(PSUICollectionView *)collectionView viewForSupplementaryElementOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)indexPath {
    if([kind isEqualToString:PSTCollectionElementKindSectionHeader])
    {
        Header *header = [collectionView dequeueReusableSupplementaryViewOfKind:PSTCollectionElementKindSectionHeader withReuseIdentifier:@"headerTitle" forIndexPath:indexPath];
        HDWServerModel *server = [_ecsModel serverAtIndex:indexPath.section];
        [header loadFromServer:server];
        
        //modify your header
        return header;
    }
    
    return nil;
}

- (void)collectionView:(PSUICollectionView *)collectionView didSelectItemAtIndexPath:(NSIndexPath *)indexPath {
    [self performSegueWithIdentifier: @"showVideo" sender: [collectionView cellForItemAtIndexPath:indexPath]];
}

- (void) prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([[segue identifier] isEqualToString:@"showVideo"]) {
        NSArray* selectedItems = [self.collectionView indexPathsForSelectedItems];
        NSIndexPath *indexPath = [selectedItems objectAtIndex:0];
        
        HDWCameraModel *camera = [_ecsModel cameraAtIndexPath: indexPath];
        [[segue destinationViewController] setCamera:camera];
    }
}

#pragma mark - Split view

- (void)splitViewController:(UISplitViewController *)splitController willHideViewController:(UIViewController *)viewController withBarButtonItem:(UIBarButtonItem *)barButtonItem forPopoverController:(UIPopoverController *)popoverController {
    barButtonItem.title = NSLocalizedString(@"Systems", @"Systems");
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

@end
