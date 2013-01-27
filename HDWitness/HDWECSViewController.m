//
//  HDWECSController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWECSViewController.h"
#include "HDWEcsConfig.h"

@interface HDWECSViewController ()

@end

@implementation HDWECSViewController

- (UIColor *) deepBlueColor
{
    return [UIColor colorWithRed:0x28/255.0f green:0x42/255.0f blue:0x75/255.0f alpha:1];
}

- (void)viewWillAppear:(BOOL)animated {
    [self.tableView setEditing:YES];
}

- (id)initWithStyle:(UITableViewStyle)style
{
    self = [super initWithStyle:style];
    if (self) {
        item = nil;
    }
    return self;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    item = [[HDWEcsConfig alloc] init];
    item.name = @"ECS1";
    item.login = @"login1";

    self.dataSourceArray = @[
        @{
            @"Title" : @"Name",
            @"Placeholder": @"Name",
            @"Property": @"name"
        },
        @{
            @"Title" : @"Host",
            @"Placeholder": @"IP or hostname",
            @"Property": @"host"
        },
        @{
            @"Title" : @"Post",
            @"Placeholder": @"Port",
            @"Property": @"port"
        },
        @{
            @"Title" : @"Login",
            @"Placeholder": @"Login",
            @"Property": @"login"
        },
        @{
            @"Title" : @"Password",
            @"Placeholder": @"Password",
            @"Property": @"password"
        },
    ];
    
    // Uncomment the following line to preserve selection between presentations.
    // self.clearsSelectionOnViewWillAppear = NO;
 
    // Uncomment the following line to display an Edit button in the navigation bar for this view controller.
    // self.navigationItem.rightBarButtonItem = self.editButtonItem;
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    return self.dataSourceArray.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *kSourceCell_ID = @"SourceCell_ID";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:kSourceCell_ID];
    
    if (cell == nil) {
        cell = [[UITableViewCell alloc] init];
        
        cell.textLabel.text = self.dataSourceArray[indexPath.row][@"Title"];
        cell.textLabel.textColor = [UIColor blackColor];
        cell.textLabel.font = [UIFont boldSystemFontOfSize:12.0];
        cell.editing = YES;
        
        CGFloat optionalRightMargin = 10.0;
        CGFloat optionalBottomMargin = 10.0;
        UITextField *textField = [[UITextField alloc] initWithFrame:CGRectMake(110, 10, cell.contentView.frame.size.width - 110 - optionalRightMargin, cell.contentView.frame.size.height - 10 - optionalBottomMargin)];
        textField.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
        textField.font = [UIFont systemFontOfSize:12.0];
        textField.backgroundColor = [UIColor clearColor];
        textField.textColor = [self deepBlueColor];
        textField.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        textField.autocorrectionType = UITextAutocorrectionTypeNo;
        
        textField.placeholder = self.dataSourceArray[indexPath.row][@"Placeholder"];
        textField.text = [item valueForKey: self.dataSourceArray[indexPath.row][@"Property"]];
        
        [cell.contentView addSubview:textField];
    }
    
    return cell;
}

// Override to support conditional editing of the table view.
- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
    // Return NO if you do not want the specified item to be editable.
    return NO;
}

/*
// Override to support editing the table view.
- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
{
    if (editingStyle == UITableViewCellEditingStyleDelete) {
        // Delete the row from the data source
        [tableView deleteRowsAtIndexPaths:@[indexPath] withRowAnimation:UITableViewRowAnimationFade];
    }   
    else if (editingStyle == UITableViewCellEditingStyleInsert) {
        // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view
    }   
}
*/

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

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    // Navigation logic may go here. Create and push another view controller.
    /*
     <#DetailViewController#> *detailViewController = [[<#DetailViewController#> alloc] initWithNibName:@"<#Nib name#>" bundle:nil];
     // ...
     // Pass the selected object to the new view controller.
     [self.navigationController pushViewController:detailViewController animated:YES];
     */
}

@end
