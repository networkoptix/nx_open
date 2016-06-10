'use strict';

var SettingsPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.email = 'noptixqa+owner@gmail.com';
    this.password = 'qweasd123';

    this.portInput = element(by.model('settings.port'));

    this.dialog = element(by.css(".modal-dialog"));
    this.dialogButtonOk = element(by.buttonText("Ok"));
    this.dialogButtonClose = element(by.buttonText("Close"));

    this.saveButton = element(by.buttonText("Save"));
    this.mustBeEqualSpan = element(by.id('mustBeEqualSpan'));
    this.mergeButton = element(by.buttonText("Merge Systems"));

    this.mediaServersLinks = element.all(by.repeater("server in mediaServers")).all(by.tagName('a'));
    this.currentMediaServer = element(by.css('ul.serversList')).element(by.cssContainingText('li', '(current)'));
    this.currentMediaServerLink = this.currentMediaServer.element(by.css('a'));

    this.connectToCloudButton = element(by.buttonText('Connect to Nx Cloud'));
    this.createCloudAccButton = element(by.linkText('Create Nx Cloud Account'));
    this.goToCloudAccButton = element(by.linkText('Go to Nx Cloud Portal'));
    this.cloudEmailInput = this.dialog.element(by.model('settings.cloudEmail'));
    this.cloudPasswordInput = this.dialog.element(by.model('settings.cloudPassword'));
    this.dialogConnectButton = this.dialog.element(by.buttonText('Connect System'));
    this.dialogDisconnectButton = this.dialog.element(by.buttonText('Disconnect System'));
    this.dialogCancelButton = this.dialog.element(by.buttonText('Cancel'));
    this.cloudDialogCloseButton = this.dialog.element(by.css('.close'));
    this.dialogMessage = this.dialog.element(by.css('.alert'));

    this.disconnectFromCloudButton = element(by.buttonText('Disconnect from Nx Cloud'));
    this.cloudSection = element(by.cssContainingText('h2', 'Cloud Connect')).element(by.xpath('..'));

    this.get = function () {
        browser.get('/#/settings');
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
        expect(this.dialogMessage.getText()).toContain('System is connected to ');
        this.cloudDialogCloseButton.click();
    };

    this.disconnectFromCloud = function (password) {
        var cloudPassword = password || this.password;

        this.disconnectFromCloudButton.click();
        this.cloudPasswordInput.sendKeys(cloudPassword);
        this.dialogDisconnectButton.click();
        expect(this.dialogMessage.getText()).toContain('System was disconnected from ');
        this.cloudDialogCloseButton.click();
    }
};

module.exports = SettingsPage;