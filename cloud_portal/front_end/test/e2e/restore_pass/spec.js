'use strict';
var RestorePasswordPage = require('./po.js');
describe('Restore password suite', function () {

    var p = new RestorePasswordPage();

    beforeEach(function() {
        p.get(p.url);
    });

    it("should demand that email field is not empty", function () {
        p.emailInput.clear();
        p.submitButton.click();
        p.checkEmailMissing();
    });

    it("should validate user email *@*.*", function () {
        p.emailInput.clear();
        p.emailInput.sendKeys('vert546 464w6345');
        p.submitButton.click();
        p.checkEmailInvalid();
    });

    xit("should validate user email *@*.* if it comes from login form", function () {
    });

    it("should not succeed, if email is not registered", function () {
        p.emailInput.clear();
        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.submitButton.click();
        p.alert.catchAlert(p.alert.alertMessages.restorePassWrongEmail, p.alert.alertTypes.danger);
    });

    p.alert.checkAlert(function(){
        p.emailInput.clear();
        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.submitButton.click();
    }, p.alert.alertMessages.restorePassWrongEmail, p.alert.alertTypes.danger, false);

    p.alert.checkAlert(function(){
        p.emailInput.clear();
        p.emailInput.sendKeys(p.helper.userEmail);
        p.submitButton.click();
    }, p.alert.alertMessages.restorePassConfirmSent, p.alert.alertTypes.success, false);

    it("generates url token for the next case", function () {
        var userEmail = p.helper.userEmail;
        p.sendLinkToEmail(userEmail);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail).then(function (email) {
            var regCode = p.getTokenFromEmail(email, userEmail);
            p.urlWithCode = (p.url + regCode);
        }));
    });

    //p.passwordField.check(p, p.urlWithCode); // need url with token from email here

    it("should be able to set new password (which is same as old)", function () {
        var userEmail = p.helper.userEmail;
        var userPassword = p.helper.userPassword;
        p.sendLinkToEmail(userEmail);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail).then(function (email) {
            var regCode = p.getTokenFromEmail(email, userEmail);
            p.get(p.url + regCode);
            p.setNewPassword(userPassword);
        }));
    });

    // TODO: write alerts for page with new password input.

    it("should set new password, login with new password", function () {
        p.sendLinkToEmail(p.helper.userEmail);

        browser.controlFlow().wait(p.helper.getEmailTo(p.helper.userEmail).then(function (email) {
            var regCode = p.getTokenFromEmail(email, p.helper.userEmail);
            p.get(p.url + regCode);
            browser.waitForAngular();
            p.setNewPassword(p.helper.userPasswordNew);
            p.helper.swapPasswords();

            p.helper.login(p.helper.userEmail, p.helper.userPassword);
            p.helper.logout();
        }));
    });

    it("should not allow to use one restore link twice", function () {
        var userEmail = p.helper.userEmail;
        var userPassword = p.helper.userPassword;
        p.sendLinkToEmail(userEmail);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail).then(function (email) {
            var regCode = p.getTokenFromEmail(email, userEmail);
            p.get(p.url + regCode);
            p.setNewPassword(userPassword);
            p.verifySecondAttemptFails(userPassword);
        }));
    });

    xit("should allow not-activated user to restore password", function () {
        expect("test").toBe("written");
    });

    xit("should make not-activated user active by restoring password", function () {
        expect("test").toBe("written");
    });

    it("should allow logged in user visit restore password page", function () {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        p.get(p.url);
        expect(p.emailInput.isPresent()).toBe(true);
        p.helper.logout();
    });

    it("should log user out if he visits restore password link from email", function () {
        var userEmail = p.helper.userEmail;
        var userPassword = p.helper.userPassword;
        browser.sleep(1500);
        p.helper.login(userEmail, userPassword);
        p.get(p.url);
        p.sendLinkToEmail(userEmail);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail).then(function (email) {
            var regCode = p.getTokenFromEmail(email, userEmail);
            p.get(p.url + regCode);
            expect(p.newPasswordInput.isPresent()).toBe(true); // due to current bug in portal, this fails
            p.get("/");
            expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false);
        }));
    });

    it("restores password to qweasd123", function () {
        var userEmail = p.helper.userEmail;
        var userPassword = p.helper.basePassword;
        p.sendLinkToEmail(userEmail);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail).then(function (email) {
            var regCode = p.getTokenFromEmail(email, userEmail);
            p.get(p.url + regCode);
            p.setNewPassword(userPassword);
        }));
    });
});