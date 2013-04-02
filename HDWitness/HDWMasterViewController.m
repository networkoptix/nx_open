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
    if (!_objects) {
        _objects = [[NSMutableArray alloc] init];
    }
    
	// Do any additional setup after loading the view, typically from a nib.
    self.navigationItem.leftBarButtonItem = self.editButtonItem;

    UIBarButtonItem *addButton = [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemAdd target:self action:@selector(insertNewObject:)];
    self.navigationItem.rightBarButtonItem = addButton;
    self.detailViewController = (HDWDetailViewController *)[[self.splitViewController.viewControllers lastObject] topViewController];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

-(void)loadSettings {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults synchronize];
    
    NSData *encodedObject = [defaults objectForKey:@"ecsconfigs"];
    _objects = (NSMutableArray*)[NSKeyedUnarchiver unarchiveObjectWithData: encodedObject];
}

-(void)saveSettings {
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSData *encodedObjects = [NSKeyedArchiver archivedDataWithRootObject:_objects];
    [defaults setObject:encodedObjects forKey:@"ecsconfigs"];
    [defaults synchronize];
}

- (void)insertNewObject:(id)sender
{
    NSUInteger newIndex = _objects.count;
    HDWECSConfig *ecsConfig = [HDWECSConfig defaultConfig];
    [_objects insertObject:ecsConfig atIndex:newIndex];
    [self saveSettings];
    
    NSIndexPath *indexPath = [NSIndexPath indexPathForRow:newIndex inSection:0];
    [self.tableView insertRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationAutomatic];
    
    HDWECSViewController *ecsViewController = [[HDWECSViewController alloc] initWithConfig:ecsConfig];
    [self.navigationController pushViewController:ecsViewController animated:YES];
}

#pragma mark - Table View

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return _objects.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:@"Cell" forIndexPath:indexPath];

    HDWECSConfig *object = _objects[indexPath.row];
    cell.textLabel.text = [object name];
    
    return cell;
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
    // Return NO if you do not want the specified item to be editable.
    return YES;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        [_objects removeObjectAtIndex:indexPath.row];
        [self saveSettings];
        [tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationFade];
    } else if (editingStyle == UITableViewCellEditingStyleInsert) {
        // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view.
    }
}

/*
// Override to support rearranging the table view.
- (void)tableView:(UITableView *)tableView moveRowAtIndexPath:(NSIndexPath *)fromIndexPath toIndexPath:(NSIndexPath *)toIndexPath
{
}
*/

/*
// Override to support conditional rearranging of the table view.
- (BOOL)tableView:(UITableView *)tableView canMoveRowAtIndexPath:(NSIndexPath *)indexPath
{
    // Return NO if you do not want the item to be re-orderable.
    return YES;
}
*/

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPad) {
        HDWECSConfig *object = _objects[indexPath.row];
        self.detailViewController.ecsConfig = object;
    }
}

- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender
{
    if ([[segue identifier] isEqualToString:@"showDetail"]) {
        NSIndexPath *indexPath = [self.tableView indexPathForSelectedRow];
        HDWECSConfig *object = _objects[indexPath.row];
        [[segue destinationViewController] setEcsConfig:object];
    } else if ([[segue identifier] isEqualToString:@"showConfig"]) {
        NSIndexPath *indexPath = [self.tableView indexPathForCell:sender];
        HDWECSConfig *ecsConfig = _objects[indexPath.row];
        [[segue destinationViewController] setConfig:ecsConfig];
    }
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    [self saveSettings];
    [self.tableView reloadData];
}

@end
