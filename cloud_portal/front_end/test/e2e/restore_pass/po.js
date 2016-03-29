'use strict';

var RestorePasswordPage;
RestorePasswordPage = function () {

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    var PasswordSuite = require('../password_check.js');
    this.passwordField = new PasswordSuite();

    this.url = '/#/restore_password/';

    this.emailSubject = this.helper.emailSubjects.restorePass;

    this.get = function (url) {
        browser.get(url);
        browser.waitForAngular();
    };

    this.checkEmailMissing = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.checkEmailInvalid = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.emailInput = element(by.model('data.email'));
    this.emailInputWrap = this.emailInput.element(by.xpath('../..'));
    this.newPasswordInput = element(by.model('data.newPassword')).element(by.css('input[type=password]'));
    this.submitButton = element(by.buttonText('Restore password'));
    this.savePasswordButton = element(by.buttonText('Save password'));

    this.sendLinkToEmail = function(email) {
        this.emailInput.clear();
        this.emailInput.sendKeys(email);
        this.submitButton.click();
        //this.alert.catchAlert(this.alert.alertMessages.restorePassConfirmSent, this.alert.alertTypes.success);
    };

    this.getTokenFromEmail = function(email, userEmail) {
        expect(email.subject).toContain("Restore your password");
        expect(email.headers.to).toEqual(userEmail);

        // extract registration token from the link in the email message
        var pattern = /\/static\/index.html#\/restore_password\/([\w=]+)/g;
        var regCode = pattern.exec(email.html)[1];
        console.log(regCode);
        return regCode;
    };

    this.setNewPassword = function(newPassword) {
        expect(this.newPasswordInput.isPresent()).toBe(true);
        this.newPasswordInput.sendKeys(newPassword);
        this.savePasswordButton.click();
        this.alert.catchAlert( this.alert.alertMessages.restorePassSuccess, this.alert.alertTypes.success);
    };

    this.verifySecondAttemptFails = function(newPassword) {
        browser.refresh();
        this.newPasswordInput.sendKeys(newPassword);
        this.savePasswordButton.click();
        this.alert.catchAlert( this.alert.alertMessages.restorePassWrongCode, this.alert.alertTypes.danger);
    };

    this.urlWithCode = "";
    this.prepareToPasswordCheck = function(){
    };
};

module.exports = RestorePasswordPage;