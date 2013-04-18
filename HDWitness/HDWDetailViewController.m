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

enum CameraStatus {
    Offline,
    Unauthorized,
    Online,
    Recording
};

enum MessageType {
    Initial                 = 0,
    Ping                    = 1,
    
    ResourceChange          = 2,
    ResourceDelete          = 3,
    ResourceStatusChange    = 4,
    ResourceDisabledChange  = 5,
    
    License                 = 6,
    CameraServerItem        = 7,
    
    BusinessRuleChange      = 8,
    BusinessRuleDelete      = 9,
    
    BroadcastBusinessAction = 10
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
    AFJSONRequestOperation *_requestOperation;
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
    if (server.status.intValue == Online) {
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

- (void)webSocket:(SRWebSocket *)webSocket didReceiveMessage:(id)rawMessage {
    NSString *messageString = (NSString*)rawMessage;
    NSData *messageData = [messageString dataUsingEncoding:NSUTF8StringEncoding];
    
    NSError* error;
    NSDictionary *message = [NSJSONSerialization JSONObjectWithData:messageData options:kNilOptions error:&error];
    
    int messageType = [message[@"type"] intValue];
    NSString *objectName = message[@"objectName"];

    NSMutableIndexSet *newServers = [NSMutableIndexSet indexSet];
    NSMutableArray *newCameras = [NSMutableArray array];
    
    NSMutableArray *removedCameras = [NSMutableArray array];
    NSMutableIndexSet *removedServers = [NSMutableIndexSet indexSet];

    if (messageType == ResourceChange) {
        NSDictionary *resource = message[@"resource"];
        
        // resource changed
        if ([objectName isEqual: @"Server"]) {
            HDWServerModel* server = [[HDWServerModel alloc] initWithDict:resource andECS:_ecsModel];
            
            NSUInteger index = [_ecsModel addOrUpdateServer:server];
            if (index != NSNotFound) {
                [newServers addIndex:index];
            }
        } else if ([objectName isEqual: @"Camera"]) {
            HDWCameraModel* camera = [[HDWCameraModel alloc] initWithDict:resource andServer:[_ecsModel findServerById:resource[@"parentId"]]];
            
            NSIndexPath* indexPath = [_ecsModel addOrReplaceCamera:camera];
            if (indexPath) {
                [newCameras addObject:indexPath];
            }
        }
    } else if (messageType == ResourceDelete) {
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
    } else if (messageType == ResourceStatusChange) {
        if ([message objectForKey:@"parentId"]) {
            HDWCameraModel *camera = [_ecsModel findCameraById:message[@"resourceId"] atServer:message[@"parentId"]];
            [camera setStatus:message[@"status"]];
        } else {
            HDWServerModel *server = [_ecsModel findServerById:message[@"resourceId"]];
            [server setStatus:message[@"status"]];
        }
    } else if (messageType == ResourceDisabledChange) {
        HDWCameraModel *camera = [_ecsModel findCameraById:message[@"resourceId"] atServer:message[@"parentId"]];
        int disabled = ((NSString*)message[@"disabled"]).intValue;
        if (disabled) {
            NSIndexPath *indexPath = [_ecsModel indexPathOfCameraWithId:message[@"resourceId"] andServerId:message[@"parentId"]];
            [camera setDisabled:YES];
            
            [removedCameras addObject:indexPath];
        } else {
            [camera setDisabled:NO];
            NSIndexPath *indexPath = [_ecsModel indexPathOfCameraWithId:message[@"resourceId"] andServerId:message[@"parentId"]];
            
            [newCameras addObject:indexPath];
        }
        
    }

    [self.collectionView performBatchUpdates:^{
        [self.collectionView deleteItemsAtIndexPaths:removedCameras];
        [self.collectionView deleteSections:removedServers];
        
        [self.collectionView insertSections:newServers];
        [self.collectionView insertItemsAtIndexPaths:newCameras];

//        [self.collectionView reloadItemsAtIndexPaths:indexPaths];
        [self.collectionView reloadData];
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

- (void)connectRequestFinished: (id)JSON {
    NSString *ecsVersion = JSON[@"version"];
    if ([ecsVersion hasPrefix:@"1.5."]) {
        _state = State_Connected;
    } else {
        _state = State_IncompatibleEcsVersion;
    }
    
    [self performNextStepWithObject:nil];
}

- (void)resourceRequestFinished: (id)JSON {
    [self _loadResourcesFromJSON:JSON toModel:_ecsModel];
    [self.collectionView reloadData];

    _state = State_GotResources;
    [self performNextStepWithObject:nil];
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

- (void)requestEntityForResourceKind:(NSString*)resourceKind success:(SEL)onSuccess statusMessage:(NSString*)statusMessage errorMessage:(NSString*)errorMessage {
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

- (void)performNextStepWithObject: (id)object {
    switch (_state) {
        case State_NetworkFailed: {
            break;
        }
            
        case State_Initial: {
            [self requestEntityForResourceKind:@"connect" success:@selector(connectRequestFinished:) statusMessage:@"Connecting to Enterprise Controller..."errorMessage:@"Can't connect to the System."];
            break;
        }
            
        case State_Connected: {
            [self requestEntityForResourceKind:@"resource" success:@selector(resourceRequestFinished:) statusMessage:@"Requesting Enterprise Controller resources..." errorMessage:@"Can't get resources."];
            break;
        }
            
        case State_IncompatibleEcsVersion: {
            [[[UIAlertView alloc] initWithTitle:@"Error" message:@"This version of ECS is incompatible with this software" delegate:self cancelButtonTitle:@"OK" otherButtonTitles:nil] show];
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
    [self performNextStepWithObject:nil];
    
    
//    UIActivityIndicatorView *spinner = [[UIActivityIndicatorView alloc] initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleWhiteLarge];
//    spinner.center = self.view.center;
//    spinner.hidesWhenStopped = YES;
    
//    [self.view addSubview:spinner];
//    [spinner startAnimating];
//
//    [SVProgressHUD showSuccessWithStatus:@"Connected"];
    
//    __block float progress = 0.0;
//    dispatch_queue_t concurrentQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
//    dispatch_async(concurrentQueue, ^{
//
//        if (_resourcesRequestOperation.isExecuting) {
//            dispatch_async(concurrentQueue, )
//        }
//        self.picker = [[UIImagePickerController alloc] init];
//        self.picker.delegate = self;
//        self.picker.sourceType = UIImagePickerControllerSourceTypePhotoLibrary;
//        self.picker.allowsEditing = NO;
        
        // 4) Present picker in main thread
//        dispatch_async(dispatch_get_main_queue(), ^{
//            [SVProgressHUD showProgress:progress];
//            progress += 0.001;
////            [self.navigationController presentModalViewController:_picker animated:YES];
//            if (progress >= 1)
//                [SVProgressHUD dismiss];
//        });
    
//    });
//    [_resourcesRequestOperation start];
    
    
    NSURL *messageUrl = [NSURL URLWithString:@"/websocket/" relativeToURL:_baseUrl];
    NSMutableURLRequest *messageRequest = [NSMutableURLRequest requestWithURL:messageUrl cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:0];
    
    NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", messageUrl.user, messageUrl.password];
    [messageRequest addValue:[NSString stringWithFormat:@"Basic %@", [basicAuthCredentials base64Encode]] forHTTPHeaderField: @"Authorization"];
    
    _socket = [[SRWebSocket alloc] initWithURLRequest:messageRequest];
    [_socket setDelegate:self];
    [_socket open];
    
    if (self.ecsConfig) {
        self.detailDescriptionLabel.text = [self.ecsConfig description];
    }
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
    
    return server.cameraCount;
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
        case Offline: {
            [cell.imageView setImageWithContentsOfFile:@"camera_offline.png"];
            break;
        }
            
        case Unauthorized: {
            [cell.imageView setImageWithContentsOfFile:@"camera_unauthorized.png"];
            break;
        }
            
        case Online:
        case Recording: {
            [cell.imageView setImageWithContentsOfFile:@"camera.png"];
            break;
        }	
    }
    
    if (camera.status.intValue == Online || camera.status.intValue == Recording) {
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
