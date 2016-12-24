'use strict';

var SettingsPage = function () {
    var p = this;
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.email = 'noptixqa+owner@gmail.com';
    this.password = 'qweasd123';

    this.portInput = element(by.model('settings.port'));

    this.dialog = element(by.css(".modal-dialog"));
    this.dialogButtonOk = element(by.buttonText("Ok"));
    this.dialogButtonClose = element(by.buttonText("Close"));

    this.saveButton = element(by.buttonText("Save"));
    this.changePortButton = element(by.buttonText("Change port"));
    this.mustBeEqualSpan = element(by.id('mustBeEqualSpan'));
    this.mergeButton = element(by.buttonText("Merge Systems"));

    this.mediaServersLinks = element.all(by.repeater("server in mediaServers")).all(by.tagName('a'));
    this.currentMediaServer = element(by.cssContainingText('div', '(current)'));
    this.currentMediaServerLink = this.currentMediaServer.element(by.css('a'));

    this.connectToCloudButton = element(by.buttonText('Connect to Nx Cloud'));
    this.createCloudAccButton = element(by.linkText('Create Nx Cloud Account'));
    this.goToCloudAccButton = element(by.linkText('Go to Nx Cloud Portal'));
    this.cloudEmailInput = this.dialog.element(by.model('settings.cloudEmail'));
    this.cloudPasswordInput = this.dialog.element(by.model('settings.cloudPassword'));
    this.oldCloudPasswordInput = this.dialog.element(by.model('settings.oldPassword'));
    this.dialogConnectButton = this.dialog.element(by.buttonText('Connect System'));
    this.dialogDisconnectButton = this.dialog.element(by.buttonText('Disconnect'));
    this.dialogCancelButton = this.dialog.element(by.buttonText('Cancel'));
    this.cloudDialogCloseButton = this.dialog.element(by.css('.close'));

    this.disconnectFromCloudButton = element(by.buttonText('Disconnect from Nx Cloud'));
    this.cloudSection = element(by.cssContainingText('h3', 'Cloud')).element(by.xpath('..'));

    this.get = function () {
        browser.get('/#/settings');
        browser.waitForAngular();
    };
    this.getSysTab = function () {
        p.get();
        p.helper.getTab("System").click();
    };

    this.setPort = function (port) {

        //Hack from https://github.com/angular/protractor/issues/562
        var length = 20;
        var backspaceSeries = '';
        for (var i = 0; i < length; i++) {
            backspaceSeries += protractor.Key.BACK_SPACE;
        }
        this.portInput.sendKeys(backspaceSeries);

        this.portInput.clear();

        this.portInput.sendKeys(port);
    };

    this.connectToCloud = function (email, password) {
        var cloudEmail = email || this.email;
        var cloudPassword = password || this.password;

        this.connectToCloudButton.click();
        this.cloudEmailInput.sendKeys(cloudEmail);
        this.cloudPasswordInput.sendKeys(cloudPassword);
        this.dialogConnectButton.click();
        browser.sleep(4000);
        // expect(this.dialog.getText()).toContain('System is connected to ');
        // this.cloudDialogCloseButton.click();
    };

    this.disconnectFromCloud = function (password) {
        var cloudPassword = password || this.password;
        p.helper.checkPresent(p.disconnectFromCloudButton).then(null, function () {
            p.get();
            p.helper.getTab('System').click();
            p.helper.checkPresent(p.disconnectFromCloudButton).then(null, function () {
                p.connectToCloud();
            });
        });
        p.disconnectFromCloudButton.click();
        p.oldCloudPasswordInput.sendKeys(cloudPassword);
        p.dialogDisconnectButton.click();
        //expect(p.dialogMessage.getText()).toContain('System was disconnected from ');

        browser.sleep(4000);
        //  If no local admin existed, add one
        p.helper.checkPresent(p.dialog).then( function() {
            p.helper.checkContainsString(p.dialog, 'Create local administrator').then( function () {
                p.dialog.element(by.css('[name=password]')).sendKeys(cloudPassword);
                p.dialog.element(by.css('[name=localPasswordConfirmation]')).sendKeys(cloudPassword);
            }, function() {});
        }, function () { // if dialog is not present
            p.helper.login(p.helper.admin, p.helper.password)
        });
        browser.refresh();
    }
};

module.exports = SettingsPage;