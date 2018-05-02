'use strict';
var RestorePasswordPage = require('./po.js');
fdescribe('Restore password page', function () {

    var p = new RestorePasswordPage();

    beforeEach(function() {
        p.helper.get(p.helper.urls.restore_password);
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

    it("restores password", function() {
        p.helper.restorePassword(p.helper.userEmail, p.helper.userPassword);
    });

    it("should be able to set new password (which is same as old), redirect", function () {
        var userEmail = p.helper.userEmail;
        var userPassword = p.helper.userPassword;
        p.sendLinkToEmail(userEmail);
        expect(browser.getCurrentUrl()).toContain("/restore_password/sent");

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail, p.emailSubject).then(function (email) {
            var regCode = p.getTokenFromEmail(email, userEmail);
            p.helper.get(p.helper.urls.restore_password +  '/' + regCode);
            p.setNewPassword(userPassword);
            expect(browser.getCurrentUrl()).toContain("/restore_password/success");
        }));
    });

    it("should not allow to access /restore_password/sent /restore_password/success by direct input", function () {
        p.helper.get('/restore_password/sent');
        expect(browser.getCurrentUrl()).not.toContain("/restore_password/sent");
        p.helper.get('/restore_password/success');
        expect(browser.getCurrentUrl()).not.toContain("/restore_password/success");
    });

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
        p.helper.get(p.helper.urls.restore_password); 
        p.getRestorePassLink(userEmail).then(function(url) {
            p.helper.get(url);
            p.setNewPassword(p.helper.userPasswordNew);
            p.helper.login(userEmail, p.helper.userPasswordNew);
            p.helper.logout();
        });
    });

    it("should allow logged in user visit restore password page", function () {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        p.helper.get(p.helper.urls.restore_password); 
        expect(p.emailInput.isPresent()).toBe(true);
        p.helper.logout();
    });

    it("should log user out if he visits restore password link from email", function () {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        p.helper.get(p.helper.urls.restore_password); 
        p.getRestorePassLink(p.helper.userEmail).then(function(url) {
            browser.get(url);
            browser.sleep(1000);

            // Log out if logged in
            p.helper.checkPresent(p.helper.forms.logout.alreadyLoggedIn).then( function () {
                p.helper.forms.logout.logOut.click()
            }, function () {});
        });
        browser.sleep(1000);
        p.setNewPassword(p.helper.userPassword);
        p.helper.get();
        expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should handle click I forgot my password link at restore password page", function () {
        p.helper.get(p.helper.urls.restore_password);
        browser.sleep(100);
        p.helper.forms.login.openLink.click();
        p.iForgotPasswordLink.click();
        expect(browser.getCurrentUrl()).toContain(p.helper.urls.restore_password);
        expect(p.helper.forms.login.dialog.isPresent()).toBe(false);
        expect(p.emailInput.isDisplayed()).toBe(true);
    });
});