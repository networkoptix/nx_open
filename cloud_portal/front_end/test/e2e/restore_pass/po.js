'use strict';

var RestorePasswordPage;
RestorePasswordPage = function () {

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    var PasswordSuite = require('../password_check.js');
    this.passwordField = new PasswordSuite();

    this.get = function () {
        browser.get('/');
        browser.waitForAngular();
        browser.sleep(500);
        this.loginButton.click();
    };

     this.checkEmailMissing = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.checkEmailInvalid = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.iForgotPasswordLink = element(by.linkText('I forgot my password'));

    this.htmlBody = element(by.css('body'));

    this.restoreEmailInput = element(by.model('data.email'));
    this.newPasswordInput = element(by.model('data.newPassword')).element(by.css('input[type=password]'));
    this.submitButton = element(by.buttonText('Restore password'));
    this.savePasswordButton = element(by.buttonText('Save password'));
};

module.exports = RestorePasswordPage;