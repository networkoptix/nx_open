'use strict';

var Page = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    var p = this;

    this.logoutButton = element(by.css('[title=Logout]'));
    this.mediaServersLinks = element.all(by.repeater('server in mediaServers')).all(by.tagName('a'));
    this.resetButton = element(by.buttonText('Restore factory defaults'));

    this.dialog = element(by.css('.modal-dialog'));
    this.oldPasswordInput = this.dialog.element(by.model('settings.oldPassword'));
    this.cancelButton = this.dialog.element(by.buttonText('Cancel'));
    this.detachButton = this.dialog.element(by.buttonText('Restore factory defaults'));
    this.closeButton = this.dialog.element(by.buttonText('Close'));

    this.setupDialog = element(by.cssContainingText('.panel-heading', 'Configure new server')).element(by.xpath('../..'));

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
    this.autoDiscovCheckbox = this.setupDialog.element(by.id('autoDiscoveryEnabled'));
    this.statisticsCheckbox = this.setupDialog.element(by.id('statisticsAllowed'));

    this.mergeWithExisting = this.setupDialog.element(by.linkText('Merge server with existing system'));
    this.remoteSystemInput = this.setupDialog.element(by.model('settings.remoteSystem'));
    this.remoteLoginInput = this.setupDialog.element(by.model('settings.remoteLogin'));
    this.remotePasswordInput = this.setupDialog.element(by.model('settings.remotePassword'));

    this.systemTypeRightButton = this.setupDialog.element(by.css('.pull-right'));
    this.useCloudAccButton = this.setupDialog.element(by.buttonText('Use existing'));
    this.cloudEmailInput = this.setupDialog.element(by.model('settings.cloudEmail'));
    this.cloudPassInput = this.setupDialog.element(by.model('settings.cloudPassword'));

    this.toggleCheckbox = function(checkbox) {
        checkbox.isSelected().then( function(isSelected) { // get the initial state of checkbox
            checkbox.click();
            if(isSelected) expect(checkbox.isSelected()).toBe(false);
            else expect(checkbox.isSelected()).toBe(true);
        });
    };

    this.get = function () {
        browser.get('/#/settings');
    };

    this.triggerSetupWizard = function() {
        this.resetButton.click();
        this.oldPasswordInput.sendKeys(this.helper.password);
        this.detachButton.click();
        p.dialog.getText().then(function(text) {
            if(p.helper.isSubstr(text, 'Wrong password')) {
                p.closeButton.click();
                browser.sleep(500); // otherwise we look for the button before dialog is closed -> exception
                p.resetButton.click();
                p.oldPasswordInput.sendKeys('admin');
                p.detachButton.click();
            }
        });

        this.helper.waitIfNotPresent(this.setupDialog, 10000);
        browser.refresh();
    }
};

module.exports = Page;