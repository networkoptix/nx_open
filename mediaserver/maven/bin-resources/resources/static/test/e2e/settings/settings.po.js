'use strict';

var AngularHomepage = function () {
    this.systemNameInput = element(by.model('settings.systemName'));
    this.portInput = element(by.model('settings.port'));
    this.passwordInput = element(by.model('settings.password'));
    this.confirmPasswordInput= element(by.model('confirmPassword'));

    this.mustBeEqualSpan = element(by.id('mustBeEqualSpan'));

    this.get = function () {
        browser.get('/#/settings');
    };

    this.setSystemName = function (systemName) {
        this.systemNameInput.sendKeys(systemName);
    };

    this.setPort = function (port) {
        this.portInput.sendKeys(port);
    };

    this.setPassword = function (password) {
        this.passwordInput.sendKeys(password);
    };

    this.setConfirmPassword = function (confirmPassword) {
        this.confirmPasswordInput.sendKeys(confirmPassword);
    };
};

module.exports = AngularHomepage;