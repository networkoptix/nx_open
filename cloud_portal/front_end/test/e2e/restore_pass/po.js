'use strict';

var RestorePasswordPage;
RestorePasswordPage = function () {
    var p = this;

    var Helper = require('../helper.js');
    this.helper = new Helper();

    var AlertSuite = require('../alerts_check.js');
    this.alert = new AlertSuite();

    var PasswordSuite = require('../password_check.js');
    this.passwordField = new PasswordSuite();

    var self = this;

    this.emailSubject = this.helper.emailSubjects.restorePass;

    this.checkEmailMissing = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.checkEmailInvalid = function () {
        expect(this.emailInput.getAttribute('class')).toContain('ng-invalid');
        expect(this.emailInputWrap.getAttribute('class')).toContain('has-error');
    };

    this.iForgotPasswordLink = p.helper.forms.login.forgotPasswordLink;
    this.emailInput = element(by.model('data.email'));
    this.emailInputWrap = this.emailInput.element(by.xpath('../..'));
    this.passwordInput = element(by.model('data.newPassword')).element(by.css('input[type=password]'));
    this.submitButton = p.helper.forms.restorePassEmail.submitButton;
    this.savePasswordButton = p.helper.forms.restorePassPassword.submitButton;

    this.messageLoginLink = p.helper.forms.restorePassPassword.messageLoginLink;

    this.passCheck = {
        input: this.passwordInput,
        submit: this.savePasswordButton
    };

    this.sendLinkToEmail = function(email) {
        this.emailInput.clear();
        this.emailInput.sendKeys(email);
        this.submitButton.click();
        expect(element(by.css('.process-success')).isDisplayed()).toBe(true);
        expect(element(by.css('.process-success')).getText()).toContain(this.alert.alertMessages.restorePassConfirmSent);
    };

    this.getTokenFromEmail = function(email, userEmail) {
        expect(email.subject).toContain("Restore your password");
        expect(email.headers.to).toEqual(userEmail);

        // extract registration token from the link in the email message
        var pattern = new RegExp("restore_password/([\\w=]+)", "g");
        var regCode = pattern.exec(email.html)[1];
        console.log(regCode);
        return regCode;
    };

    this.setNewPassword = function(newPassword) {
        expect(this.passwordInput.isPresent()).toBe(true);
        this.passwordInput.sendKeys(newPassword);
        this.savePasswordButton.click();
        expect(element(by.css('.process-success')).isDisplayed()).toBe(true);
        expect(element(by.css('.process-success')).getText()).toContain(this.alert.alertMessages.restorePassSuccess);
    };

    this.verifySecondAttemptFails = function(restorePassUrl, newPassword) {
        this.helper.get(restorePassUrl);
        this.passwordInput.sendKeys(newPassword);
        this.savePasswordButton.click();
        this.alert.catchAlert( this.alert.alertMessages.restorePassWrongCode, this.alert.alertTypes.danger);
    };

    this.getRestorePassLink = function(userEmail) {
        var deferred = protractor.promise.defer();
        this.sendLinkToEmail(userEmail);

        browser.controlFlow().wait(this.helper.getEmailTo(userEmail, this.emailSubject).then(function (email) {
            var regCode = self.getTokenFromEmail(email, userEmail);
            deferred.fulfill(self.helper.urls.restore_password + '/' + regCode);
        }));

        return deferred.promise;
    };
};

module.exports = RestorePasswordPage;