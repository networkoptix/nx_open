//
//  HDWMasterViewController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/21/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWMasterViewController.h"

#import "HDWDetailViewController.h"
#import "HDWECSViewController.h"

@interface HDWMasterViewController () {
    NSMutableArray *_objects;
}
@end

@implementation HDWMasterViewController

- (void)awakeFromNib
{
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        self.clearsSelectionOnViewWillAppear = NO;
        self.contentSizeForViewInPopover = CGSizeMake(320.0, 600.0);
    }
    [super awakeFromNib];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    [self loadSettings];
    
	// Do any additional setup after loading the view, typically from a nib.
    
    self.navigationItem.leftBarButtonItem = self.editButtonItem;
    self.detailViewController = (HDWDetailViewController *)[[self.splitViewController.viewControllers lastObject] topViewController];
}

- (void)addOrReplaceECSConfig: (HDWECSConfig*)ecsConfig atIndex:(NSUInteger)index {
    if (index < _objects.count)
        [_objects replaceObjectAtIndex:index withObject:ecsConfig];
    else
        [_objects insertObject:ecsConfig atIndex:index];
    
    [self saveSettings];
    [self.tableView reloadData];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

- (void)loadSettings {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults synchronize];
    
    NSData *encodedObject = [defaults objectForKey:@"ecsconfigs"];
    if (encodedObject) {
        _objects = (NSMutableArray*)[NSKeyedUnarchiver unarchiveObjectWithData: encodedObject];
    } else {
        _objects = [[NSMutableArray alloc] init];
    }
}

- (void)saveSettings {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSData *encodedObjects = [NSKeyedArchiver archivedDataWithRootObject:_objects];
    [defaults setObject:encodedObjects forKey:@"ecsconfigs"];
    [defaults synchronize];
}

#pragma mark - Table View

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return _objects.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 60000
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell" forIndexPath:indexPath];
#else
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell"];
#endif

    HDWECSConfig *object = _objects[indexPath.row];
    cell.textLabel.text = [object name];
    
    return cell;
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath {
    // Return NO if you do not want the specified item to be editable.
    return YES;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath {
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        [_objects removeObjectAtIndex:indexPath.row];
        [self saveSettings];
        [tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationFade];
    } else if (editingStyle == UITableViewCellEditingStyleInsert) {
        // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view.
    }
}

// Override to support rearranging the table view.
- (void)tableView:(UITableView *)tableView moveRowAtIndexPath:(NSIndexPath *)fromIndexPath toIndexPath:(NSIndexPath *)toIndexPath {
    [_objects exchangeObjectAtIndex:fromIndexPath.row withObjectAtIndex:toIndexPath.row];
}

// Override to support conditional rearranging of the table view.
- (BOOL)tableView:(UITableView *)tableView canMoveRowAtIndexPath:(NSIndexPath *)indexPath {
    // Return NO if you do not want the item to be re-orderable.
    return YES;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    if (self.editing) {
        [self performSegueWithIdentifier:@"showConfig" sender:[tableView cellForRowAtIndexPath:indexPath]];
        return;
    }
    
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        HDWECSConfig *object = _objects[indexPath.row];
        self.detailViewController.ecsConfig = object;
    }
}

- (void)tableView:(UITableView *)tableView accessoryButtonTappedForRowWithIndexPath:(NSIndexPath *)indexPath {
    [self performSegueWithIdentifier: @"showConfig" sender: [tableView cellForRowAtIndexPath: indexPath]];
}

- (BOOL)shouldPerformSegueWithIdentifier:(NSString *)identifier sender:(id)sender {
    if ([identifier isEqualToString:@"showDetail"]) {
        if (self.editing) {
            return NO;
        }
    }
    
    return YES;
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    if ([[segue identifier] isEqualToString:@"showDetail"]) {
        NSIndexPath *indexPath = [self.tableView indexPathForSelectedRow];
        HDWECSConfig *object = _objects[indexPath.row];
    
        if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
            [[segue destinationViewController] setEcsConfig:object];
        } else if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
            UINavigationController* navigationViewController = [segue destinationViewController];
            HDWDetailViewController* detailViewController = (HDWDetailViewController*) navigationViewController.topViewController;
            detailViewController.title = object.name;
            [detailViewController setEcsConfig:object];
        }
    } else if ([[segue identifier] isEqualToString:@"showConfig"]) {
        NSIndexPath *indexPath = [self.tableView indexPathForCell:sender];
        HDWECSViewController *ecsViewController = (HDWECSViewController *)[[segue destinationViewController] topViewController];

        ecsViewController.delegate = self;
        
        if (indexPath) {
            ecsViewController.config = _objects[indexPath.row];
            ecsViewController.index = [NSNumber numberWithInteger:indexPath.row];
        } else {
            ecsViewController.config = [HDWECSConfig defaultConfig];
            ecsViewController.index = [NSNumber numberWithInteger:_objects.count];
        }
    }
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    
    [self saveSettings];
    [self.tableView reloadData];
}

@end
