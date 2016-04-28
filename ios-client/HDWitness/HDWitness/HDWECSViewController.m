//
//  HDWECSController.m
//  HDWitness
//
//  Created by Ivan Vigasin on 1/22/13.
//  Copyright (c) 2013 Ivan Vigasin. All rights reserved.
//

#import "HDWMasterViewController.h"
#import "HDWECSViewController.h"
#import "HDWCameraModel.h"
#import "HDWECSConfig.h"

#import "Config.h"

#pragma mark - HDWECSViewController interface

@interface HDWECSViewController ()
@end

#pragma mark - HDWECSViewController implementation

@implementation HDWECSViewController

- (UIColor *) deepBlueColor {
    return [UIColor colorWithRed:0x28/255.0f green:0x42/255.0f blue:0x75/255.0f alpha:1];
}

- (IBAction)onCancel:(id)sender {
    _config = nil;
    [self.navigationController popViewControllerAnimated:YES];
}

- (void)alertView:(UIAlertView *)alertView clickedButtonAtIndex:(NSInteger)buttonIndex {
    [(UITextField *)[self.view viewWithTag:1] becomeFirstResponder];
}

- (IBAction)onSave:(id)sender {
    if ([[self delegate] countOtherECSWithSameName:_config.name atIndex:_index.intValue] > 0) {
        UIAlertView *alertView = [[UIAlertView alloc] initWithTitle:L(@"Such name already exists")
                                   message:L(@"Please choose another name.")
                                  delegate:self
                         cancelButtonTitle:L(@"OK")
                         otherButtonTitles:nil];
        [alertView show];
        
    } else {
        [[self delegate] addOrReplaceECSConfig:_config atIndex:_index.intValue];
    
        [self.navigationController popViewControllerAnimated:YES];
    }
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    
    [self.tableView setEditing:YES];
    _saveButtonItem.enabled = [_config isFilled];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    if (SYSTEM_VERSION_LESS_THAN_OR_EQUAL_TO(@"7.0"))
        [self.tableView setContentInset:UIEdgeInsetsMake(0, 0, 0, 0)];

    _dataSourceArray = @[
        @{
            @"Title" : L(@"Name"),
            @"Placeholder": L(@"Name"),
            @"Property": @"name"
        },
        @{
            @"Title" : L(@"Host"),
            @"Placeholder": L(@"IP or hostname"),
            @"Property": @"host"
        },
        @{
            @"Title" : L(@"Port"),
            @"Placeholder": L(@"Port"),
            @"Property": @"port"
        },
        @{
            @"Title" : L(@"Login"),
            @"Placeholder": L(@"Login"),
            @"Property": @"login"
        },
        @{
            @"Title" : L(@"Password"),
            @"Placeholder": L(@"Password"),
            @"Property": @"password"
        },
    ];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
    return _dataSourceArray.count;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    static NSString *kSourceCell_ID = @"SourceCell_ID";
    
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:kSourceCell_ID];
    
    if (cell == nil) {
        cell = [[UITableViewCell alloc] init];
        
        cell.textLabel.text = _dataSourceArray[indexPath.row][@"Title"];
        cell.textLabel.textColor = [UIColor blackColor];
        cell.textLabel.font = [UIFont boldSystemFontOfSize:12.0];
        cell.editing = YES;
        
        CGFloat optionalRightMargin = 10.0;
        CGFloat optionalBottomMargin = 10.0;
        UITextField *textField = [[UITextField alloc] initWithFrame:CGRectMake(110, 10, cell.contentView.frame.size.width - 110 - optionalRightMargin, cell.contentView.frame.size.height - 10 - optionalBottomMargin)];
        [textField addTarget:self action:@selector(valueChanged:) forControlEvents:UIControlEventEditingChanged];
        textField.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
        textField.font = [UIFont systemFontOfSize:12.0];
        textField.backgroundColor = [UIColor clearColor];
        textField.textColor = [self deepBlueColor];
        textField.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        textField.autocorrectionType = UITextAutocorrectionTypeNo;
        textField.autocapitalizationType = UITextAutocapitalizationTypeNone;
        textField.tag = indexPath.row + 1;
        textField.delegate = self;
        
        textField.placeholder = _dataSourceArray[indexPath.row][@"Placeholder"];
        
        NSString *property = _dataSourceArray[indexPath.row][@"Property"];
        textField.text = [_config valueForKey:property];
        
        if ([property isEqualToString:@"password"]) {
            textField.secureTextEntry = YES;
        }
        
        [cell.contentView addSubview:textField];
    }
    
    return cell;
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    NSInteger currentTag = textField.tag;
    if (currentTag < _dataSourceArray.count) {
        UITextField *nextField = (UITextField *)[self.view viewWithTag:currentTag + 1];
        [nextField becomeFirstResponder];
    } else if (currentTag == _dataSourceArray.count) {
        [self onSave:textField];
        return NO;
    }
    
    return YES;
}

- (BOOL)tableView:(UITableView *)tableView canEditRowAtIndexPath:(NSIndexPath *)indexPath
{
    return NO;
}

#pragma mark - Table view delegate

- (IBAction)valueChanged:(UITextField *)sender {
    NSInteger row = sender.tag - 1;
    [_config setValue:sender.text forKey:_dataSourceArray[row][@"Property"]];
    _saveButtonItem.enabled = [_config isFilled];
}

- (void)viewDidUnload {
    [self setSaveButtonItem:nil];
    [super viewDidUnload];
}

@end
