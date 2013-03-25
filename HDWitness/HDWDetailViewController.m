//
//  HDWDetailViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/21/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWDetailViewController.h"
#import "HDWCollectionViewCell.h"

#import "AFJSONRequestOperation.h"
#import "AFHTTPClient.h"

@interface HDWDetailViewController ()
@property (strong, nonatomic) UIPopoverController *masterPopoverController;
- (void)configureView;
@end

@interface Header : UICollectionReusableView
@property (weak, nonatomic) IBOutlet UILabel *label;
@end

@implementation Header

- (void)setText:(NSString*)text {
    self.label.text = text;
}

@end

@implementation HDWDetailViewController

- (void)webSocket:(SRWebSocket *)webSocket didReceiveMessage:(id)rawMessage
{
    NSString *messageString = (NSString*)rawMessage;
    NSData *messageData = [messageString dataUsingEncoding:NSUTF8StringEncoding];
    
    NSError* error;
    NSDictionary *message = [NSJSONSerialization JSONObjectWithData:messageData options:kNilOptions error:&error];
    
    NSNumber *messageType = message[@"type"];
    NSString *objectName = message[@"objectName"];
    
    NSMutableArray *changedCameras = [[NSMutableArray alloc] init];
    
    if ([messageType intValue] == 2) {
        NSDictionary *resource = message[@"resource"];
        
        // resource changed
        if ([objectName isEqual: @"Server"]) {
            HDWServerModel* server = [HDWServerModel alloc];
            server.serverId = resource[@"id"];
            server.name = resource[@"name"];
            server.streamingUrl = resource[@"streamingUrl"];
            
            [_servers updateServer:server];
        } else if ([objectName isEqual: @"Camera"]) {
            HDWCameraModel* camera = [HDWCameraModel alloc];
            camera.cameraId = resource[@"id"];
            camera.serverId = resource[@"parentId"];
            camera.name = resource[@"name"];
            camera.physicalId = resource[@"physicalId"];
            camera.status = resource[@"status"];
            
            [_servers updateCamera:camera];
            
            [changedCameras addObject:[_servers getIndexPathOfCameraWithId:camera.cameraId andServerId:camera.serverId]];
        }
    }
    

    [self.collectionView performBatchUpdates:^{
        [self.collectionView reloadItemsAtIndexPaths:changedCameras];
    } completion:nil];
    
    NSLog(@"Pens: %@", message);
}

#pragma mark - Managing the detail item

- (void)setEcsConfig:(id)newEcsConfig {
    if (_ecsConfig != newEcsConfig) {
        _ecsConfig = newEcsConfig;
        
        _servers = [[HDWServersModel alloc] init];
        // Update the view.
//        [self configureView];
    }

    if (self.masterPopoverController != nil) {
        [self.masterPopoverController dismissPopoverAnimated:YES];
    }        
}

- (void) onGotResultsFromEcs {
    [self.collectionView reloadData];
}

-(void) requestFinished: (id) JSON {
    NSMutableDictionary* serversDict = [[NSMutableDictionary alloc] init];
    
    NSArray *jsonServers = JSON[@"servers"];
    for (NSDictionary *jsonServer in jsonServers) {
        HDWServerModel *server = [[HDWServerModel alloc] init];
        server.serverId = jsonServer[@"id"];
        server.name = jsonServer[@"name"];
        server.streamingUrl = [NSURL URLWithString: jsonServer[@"streaming_url"]];
        
        [serversDict setObject:server forKey:server.serverId];
    }

    NSArray *jsonCameras = JSON[@"cameras"];
    for (NSDictionary *jsonCamera in jsonCameras) {
        HDWCameraModel *camera = [[HDWCameraModel alloc] init];
        camera.cameraId = jsonCamera[@"id"];
        camera.serverId = jsonCamera[@"parent_id"];
        camera.name = jsonCamera[@"name"];
        camera.physicalId = jsonCamera[@"physical_id"];
        
        HDWServerModel *server = [serversDict objectForKey: camera.serverId];
        
        camera.videoUrl = [NSURL URLWithString:[NSString stringWithFormat:@"/media/%@.mpjpeg", camera.physicalId] relativeToURL:server.streamingUrl];
        camera.thumbnailUrl = [NSURL URLWithString:[NSString stringWithFormat:@"/api/image?physicalId=%@&time=now", camera.physicalId] relativeToURL:server.streamingUrl];
        
        [server.cameras addObject: camera];
    }
    
    [_servers addServers: [serversDict allValues]];
    
    [self onGotResultsFromEcs];
    NSLog(@"finished");
}

- (void)configureView
{
    NSString* urlString = [NSString stringWithFormat:@"https://%@:%@@%@:%@/", _ecsConfig.login, _ecsConfig.password, _ecsConfig.host, _ecsConfig.port];
    NSURL *baseUrl = [NSURL URLWithString:urlString];
    
    NSURL *resourceUrl = [NSURL URLWithString:@"api/resource/" relativeToURL:baseUrl];
    NSURLRequest *resourceRequest = [NSURLRequest requestWithURL:resourceUrl];
    
    AFJSONRequestOperation *resourceOperation = [AFJSONRequestOperation JSONRequestOperationWithRequest:resourceRequest success:^(NSURLRequest *request, NSHTTPURLResponse *response, id JSON) {
        [self requestFinished: JSON];
    } failure:^(NSURLRequest *request, NSHTTPURLResponse *response, NSError *error, id JSON) {
        NSLog(@"Error: %@", error);
    }];
    [resourceOperation start];
    
    
    NSURL *messageUrl = [NSURL URLWithString:@"/events/" relativeToURL:baseUrl];
    NSMutableURLRequest *messageRequest = [NSMutableURLRequest requestWithURL:messageUrl cachePolicy:NSURLRequestReloadIgnoringCacheData timeoutInterval:0];
    
    NSString *basicAuthCredentials = [NSString stringWithFormat:@"%@:%@", messageUrl.user, messageUrl.password];
    [messageRequest addValue:[NSString stringWithFormat:@"Basic %@", AFBase64EncodedStringFromString(basicAuthCredentials)] forHTTPHeaderField: @"Authorization"];
    
    _socket = [[SRWebSocket alloc] initWithURLRequest:messageRequest];
    [_socket setDelegate:self];
    [_socket open];
    
    if (self.ecsConfig) {
        self.detailDescriptionLabel.text = [self.ecsConfig description];
    }
}

- (void)viewDidLoad
{
    [super viewDidLoad];

	// Do any additional setup after loading the view, typically from a nib.
    [self configureView];
    
    self.videoViewController = (HDWVideoViewController *)[[self.splitViewController.viewControllers lastObject] topViewController];
}

- (void)willMoveToParentViewController:(UIViewController *)parent {
    [_socket close];
    NSLog(@"willMoveToParentViewController");
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

-(NSInteger)numberOfSectionsInCollectionView:(UICollectionView *)collectionView
{
    return _servers.count;
}

-(NSInteger)collectionView:(UICollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    HDWServerModel* server = [_servers getServerAtIndex: section];
    
    return server.cameras.count;
}

-(UICollectionViewCell *)collectionView:(UICollectionView *)collectionView cellForItemAtIndexPath:(NSIndexPath *)indexPath
{
    HDWCollectionViewCell *cell = [collectionView dequeueReusableCellWithReuseIdentifier:@"MyCell" forIndexPath:indexPath];

    cell.imageView.reflectionScale = 0.5f;
    cell.imageView.reflectionAlpha = 0.25f;
    cell.imageView.reflectionGap = 10.0f;
    cell.imageView.shadowOffset = CGSizeMake(0.0f, 2.0f);
    cell.imageView.shadowBlur = 5.0f;
    cell.imageView.cornerRadius = 10.0f;

    HDWServerModel *server = [_servers getServerAtIndex: indexPath.section];
    HDWCameraModel *camera = [server.cameras objectAtIndex:indexPath.row];
   
    [cell.imageView setImageWithContentsOfURL:camera.thumbnailUrl];
    cell.labelView.text = camera.name;
    
    return cell;
}

- (UICollectionReusableView *)collectionView:(UICollectionView *)collectionView viewForSupplementaryElementOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)indexPath {
    if(kind == UICollectionElementKindSectionHeader)
    {
        Header *header = [collectionView dequeueReusableSupplementaryViewOfKind:UICollectionElementKindSectionHeader withReuseIdentifier:@"headerTitle" forIndexPath:indexPath];
        HDWServerModel *server = [_servers getServerAtIndex:indexPath.section];
        [header setText: server.name];
        
        //modify your header
        return header;
    }
    
    return nil;
}

- (void) prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([[segue identifier] isEqualToString:@"showVideo"]) {
        NSArray* selectedItems = [self.collectionView indexPathsForSelectedItems];
        NSIndexPath *indexPath = [selectedItems objectAtIndex:0];
        
        HDWCameraModel *camera = [_servers getCameraForIndexPath: indexPath];
        [[segue destinationViewController] setCamera:camera];
    }
}

#pragma mark - Split view

- (void)splitViewController:(UISplitViewController *)splitController willHideViewController:(UIViewController *)viewController withBarButtonItem:(UIBarButtonItem *)barButtonItem forPopoverController:(UIPopoverController *)popoverController
{
    barButtonItem.title = NSLocalizedString(@"Master", @"Master");
    [self.navigationItem setLeftBarButtonItem:barButtonItem animated:YES];
    self.masterPopoverController = popoverController;
}

- (void)splitViewController:(UISplitViewController *)splitController willShowViewController:(UIViewController *)viewController invalidatingBarButtonItem:(UIBarButtonItem *)barButtonItem
{
    // Called when the view is shown again in the split view, invalidating the button and popover controller.
    [self.navigationItem setLeftBarButtonItem:nil animated:YES];
    self.masterPopoverController = nil;
}

@end
