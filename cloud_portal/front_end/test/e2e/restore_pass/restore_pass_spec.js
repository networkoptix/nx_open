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

    p.passwordField.check(function(){
        var deferred = protractor.promise.defer();
        p.getRestorePassLink(p.helper.userEmail).then(function(url){
            p.helper.get(url);
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
        p.getRestorePassLink(p.helper.userEmail).then( function(url) {
            p.helper.get(url);
            p.setNewPassword(p.helper.userPassword);
            p.verifySecondAttemptFails(url, p.helper.userPassword);
            deferred.fulfill();
        });
        return deferred.promise;
    }, p.alert.alertMessages.restorePassWrongCode, p.alert.alertTypes.danger, false);

    it("should set new password, login with new password", function () {
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            p.helper.get(url);
            p.setNewPassword(p.helper.userPasswordNew);
            p.helper.login(p.helper.userEmail, p.helper.userPasswordNew);
            p.helper.logout();
            p.helper.restorePassword(p.helper.userEmail, p.helper.userPassword);
        });
    });

    it("should not allow to use one restore link twice", function () {
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            p.helper.get(url);
            p.setNewPassword(p.helper.userPassword);
            p.verifySecondAttemptFails(url, p.helper.userPassword);
        });
    });

    it("should make not-activated user active by restoring password", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.get(p.url);
        p.getRestorePassLink(userEmail).then(function(url) {
            p.helper.get(url);
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

    it("should log user out if he visits restore password link from email", function () {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        p.get(p.url);
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            /* Please leave then() in browser.get(url).then(function(){});
            * Otherwise, exception is sometimes thrown
            * The problem is: inside .then() browser.wait()/ .sleep() / .pause() do not work.
            * Thus, this hack is used to avoid UNCAUGHT_EXCEPTION
            * (see info about exception here
            * http://definitelytyped.org/docs/selenium-webdriver--selenium-webdriver/classes/webdriver.promise.controlflow.html#static-eventtype )
            */
            browser.get(url).then(function(){});
        });
        browser.sleep(1000);
        p.setNewPassword(p.helper.userPassword);
        p.get("/");
        expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false);
    });
});