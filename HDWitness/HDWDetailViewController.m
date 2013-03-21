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
#import "AZSocketIO.h"

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

#pragma mark - Managing the detail item

- (void)setEcsConfig:(id)newEcsConfig
{
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

- (void) onGotResultsFromEcs
{
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
    
    NSURL *resourceUrl = [NSURL URLWithString:@"api/resource" relativeToURL:baseUrl];
    NSURLRequest *resourceRequest = [NSURLRequest requestWithURL:resourceUrl];
    
    AFJSONRequestOperation *resourceOperation = [AFJSONRequestOperation JSONRequestOperationWithRequest:resourceRequest success:^(NSURLRequest *request, NSHTTPURLResponse *response, id JSON) {
        [self requestFinished: JSON];
    } failure:^(NSURLRequest *request, NSHTTPURLResponse *response, NSError *error, id JSON) {
        NSLog(@"Error: %@", error);
    }];
    [resourceOperation start];
    
    NSURL *messageUrl = [NSURL URLWithString:@"events/" relativeToURL:baseUrl];
    NSURLRequest *messageRequest = [NSURLRequest requestWithURL:messageUrl];
    
    AZSocketIO *socket = [[AZSocketIO alloc] initWithRequest:messageRequest];
    
    [socket setEventRecievedBlock:^(NSString *eventName, id data) {
        NSLog(@"%@ : %@", eventName, data);
    }];
    [socket connectWithSuccess:^{
        [socket emit:@"Send Me Data" args:@"cows" error:nil];
    } andFailure:^(NSError *error) {
        NSLog(@"Boo: %@", error);
    }];
    
//    AFHTTPRequestOperation *messagesOperation = [[AFHTTPRequestOperation alloc] initWithRequest:messagesRequest];
//    messagesOperation.outputStream = [NSOutputStream outputStreamToMemory];
//    [messagesOperation setDownloadProgressBlock:^(NSUInteger bytesRead, long long totalBytesRead, long long totalBytesExpectedToRead) {
//        NSLog(@"%d bytes read", bytesRead);
//    }];
//    [messagesOperation start];
    
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
    
    HDWServerModel *server = [_servers getServerAtIndex: indexPath.section];
    HDWCameraModel *camera = [server.cameras objectAtIndex:indexPath.row];
    
    NSURL *streamingUrl = server.streamingUrl;
    
    NSString* urlString = [NSString stringWithFormat:@"http://%@:%@/api/image?physicalId=%@&time=now",
                           streamingUrl.host, streamingUrl.port, camera.physicalId];
    
    NSURL *url= [NSURL URLWithString:urlString];
    NSData *data = [NSData dataWithContentsOfURL:url];
    
    cell.imageView.image = [UIImage imageWithData:data];
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
        NSLog(@"%@", selectedItems);
        
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
