'use strict';
var RestorePasswordPage = require('./po.js');
describe('Restore password suite', function () {

    var p = new RestorePasswordPage();

    beforeEach(function() {
        p.get(p.url);
    });

    beforeAll(function() {
        console.log('Restore pass start');
    });
    afterAll(function() {
        console.log('Restore pass finish');
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

    p.passwordField.check(function(){
        var deferred = protractor.promise.defer();
        p.getRestorePassPage(p.helper.userEmail).then(function(){
            browser.sleep(500);
            deferred.fulfill();
        });
        return deferred.promise;
    }, p);

    it("should be able to set new password (which is same as old)", function () {
        var userEmail = p.helper.userEmail;
        var userPassword = p.helper.userPassword;
        p.sendLinkToEmail(userEmail);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail, p.emailSubject).then(function (email) {
            var regCode = p.getTokenFromEmail(email, userEmail);
            p.get(p.url + regCode);
            p.setNewPassword(userPassword);
        }));
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        p.emailInput.clear();
        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.submitButton.click();
        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.restorePassWrongEmail, p.alert.alertTypes.danger, false);

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        p.emailInput.clear();
        p.emailInput.sendKeys(p.helper.userEmail);
        p.submitButton.click();
        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.restorePassConfirmSent, p.alert.alertTypes.success, false);

    //p.alert.checkAlert(function(){
    //    var deferred = protractor.promise.defer();
    //    p.getRestorePassPage(p.helper.userEmail).then(function(){
    //        p.setNewPassword(p.helper.userPassword);
    //        p.verifySecondAttemptFails(p.helper.userPassword);
    //        deferred.fulfill();
    //    });
    //    return deferred.promise;
    //}, p.alert.alertMessages.restorePassWrongCode, p.alert.alertTypes.danger, false);

    it("should set new password, login with new password", function () {
        p.getRestorePassPage(p.helper.userEmail).then(function() {
            p.setNewPassword(p.helper.userPasswordNew);
            p.helper.login(p.helper.userEmail, p.helper.userPasswordNew);
            p.helper.logout();
            p.helper.restorePassword(p.helper.userEmail, p.helper.userPassword);
        });
    });

    //xit("should not allow to use one restore link twice", function () {
    //    p.getRestorePassPage(p.helper.userEmail).then(function() {
    //        p.setNewPassword(p.helper.userPassword);
    //        p.verifySecondAttemptFails(p.helper.userPassword);
    //    });
    //});

    it("should make not-activated user active by restoring password", function () {
        var userEmail = p.helper.register();
        p.get(p.url);
        p.getRestorePassPage(userEmail).then(function() {
            p.setNewPassword(p.helper.userPasswordNew);
            p.helper.login(userEmail, p.helper.userPasswordNew);
            p.helper.logout();
        });
    });

    it("should allow logged in user visit restore password page", function () {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        p.get(p.url);
        expect(p.emailInput.isPresent()).toBe(true);
        p.helper.logout();
    });

    xit("should log user out if he visits restore password link from email", function () {
        p.helper.login(p.helper.userEmail, p.helper.basePassword);
        p.get(p.url);
        p.getRestorePassPage(p.helper.userEmail).then(function() {
            p.setNewPassword(p.helper.basePassword); // due to current bug in portal, this fails
            p.get("/");
            expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false);
        });
    });
});