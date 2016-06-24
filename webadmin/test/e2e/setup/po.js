'use strict';

var Page = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    var p = this;

    this.mediaServersLinks = element.all(by.repeater('server in mediaServers')).all(by.tagName('a'));
    //this.resetButton = element(by.buttonText('Reset System'));
    this.resetButton = element(by.buttonText('Restore factory defaults'));
    this.resetAndNewButton = element(by.buttonText('Disconnect Server and Create New System'));

    this.dialog = element(by.css('.modal-dialog'));
    this.oldPasswordInput = this.dialog.element(by.model('settings.oldPassword'));
    this.cancelButton = this.dialog.element(by.buttonText('Cancel'));
    //this.detachButton = this.dialog.element(by.buttonText('Create New System'));
    this.detachButton = this.dialog.element(by.buttonText('Restore factory defaults'));
    this.closeButton = this.dialog.element(by.buttonText('Close'));

    this.setupDialog = element(by.css('.modal')).element(by.css('.panel'));
    this.nextButton = this.setupDialog.element(by.cssContainingText('button', 'Next'));
    this.backButton = this.setupDialog.element(by.css('.glyphicon-arrow-left'));
    this.systemNameInput = this.setupDialog.element(by.model('settings.systemName'));
    this.skipCloud = this.setupDialog.element(by.linkText('Skip this step for now'));
    this.learnMore = this.setupDialog.element(by.linkText('Learn more'));
    this.localPasswordInput = this.setupDialog.element(by.model('ngModel'));
    //this.localPasswordInput = this.setupDialog.element(by.model('settings.localPassword'));
    this.localPasswordConfInput = this.setupDialog.element(by.model('settings.localPasswordConfirmation'));
    this.finishButton = this.setupDialog.element(by.buttonText('Finish'));
    this.advancedSysSett = this.setupDialog.element(by.linkText('Advanced system settings'));
    this.camOptimCheckbox = this.setupDialog.element(by.id('cameraSettingsOptimization'));
    this.autoDiscovCheckbox = this.setupDialog.element(by.id('serverAutoDiscoveryEnabled'));
    this.statisticsCheckbox = this.setupDialog.element(by.id('statisticsAllowed'));

    this.mergeWithExisting = this.setupDialog.element(by.linkText('Merge server with existing system'));
    this.remoteSystemInput = this.setupDialog.element(by.model('settings.remoteSystem'));
    this.remotePasswordInput = this.setupDialog.element(by.model('settings.remotePassword'));

    this.useCloudAccButton = this.setupDialog.element(by.buttonText('Use existing'));
    this.cloudEmailInput = this.setupDialog.element(by.model('settings.cloudEmail'));
    this.cloudPassInput = this.setupDialog.element(by.model('settings.cloudPassword'));

    this.getCheckboxState = function(checkbox) {
        checkbox.isSelected().then( function(isSelected) {
            return isSelected;
        });
    };
    this.toggleCheckbox = function(checkbox) {
        var isSelected = this.getCheckboxState(checkbox);
        checkbox.click();
        if(isSelected) expect(checkbox.isSelected()).toBe(false);
        else expect(checkbox.isSelected()).toBe(true);
    };

    this.get = function () {
        browser.get('/#/settings');
    };

    this.triggerSetupWizard = function() {
        this.resetButton.click();
        this.oldPasswordInput.sendKeys(this.helper.password);
        this.detachButton.click();

        // If login dialog appears, reload page.
        browser.sleep(500);
        p.helper.checkPresent(element(by.model('user.username'))).then( function() {
            browser.refresh();
            browser.sleep(1000);
        });
        this.helper.waitIfNotDisplayed(this.setupDialog, 1000);
    }
};

module.exports = Page;