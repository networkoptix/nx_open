'use strict';

var SettingsPage = function () {
    this.systemNameInput = element(by.model('settings.systemName'));
    this.portInput = element(by.model('settings.port'));

    this.passwordInput = element(by.model('password'));
    this.confirmPasswordInput= element(by.model('confirmPassword'));
    this.oldPasswordInput= element(by.model('oldPassword'));

    this.changePasswordButton = element(by.buttonText("Change password"));
    this.saveButton = element(by.buttonText("Save"));
    this.mustBeEqualSpan = element(by.id('mustBeEqualSpan'));
    this.resetButton = element(by.buttonText("Restart"));
    this.mergeButton = element(by.buttonText("Merge Systems"));

    this.mediaServersLinks = element.all(by.repeater("server in mediaServers")).all(by.tagName('a'));
    this.activeMediaServerLink = element.all(by.repeater("server in mediaServers")).all(by.css(".active>a")).first();

    this.get = function () {
        browser.get('/#/settings');
    };


    this.getServer = function () {
        browser.get('http://admin:123@192.168.56.101:9000/static/index.html#/settings');
    };
    this.getServerOnAnotherPort = function () {
        browser.get('http://admin:123@192.168.56.101:7000/static/index.html#/settings');
    };
    this.getServerWithAnotherPassword= function () {
        browser.get('http://admin:1234@192.168.56.101:9000/static/index.html#/settings');
    };


    this.setSystemName = function (systemName) {
        this.systemNameInput.clear();
        this.systemNameInput.sendKeys(systemName);
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


    this.setOldPassword = function (password) {
        this.oldPasswordInput.clear();
        this.oldPasswordInput.sendKeys(password);
    };

    this.setPassword = function (password) {
        this.passwordInput.clear();
        this.passwordInput.sendKeys(password);
    };

    this.setConfirmPassword = function (confirmPassword) {

        this.confirmPasswordInput.clear();
        this.confirmPasswordInput.sendKeys(confirmPassword);
    };
};

module.exports = SettingsPage;