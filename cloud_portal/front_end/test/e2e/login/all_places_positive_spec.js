'use strict';
var LoginPage = require('./po.js');
describe('Login with correct credentials', function () {

    var p = new LoginPage();

    beforeAll( function () {
        p.helper.get();
    });

    it("works at registration page before submit", function() {
        p.helper.get( p.helper.urls.register );
        browser.sleep(500);
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page after submit success", function() {
        p.helper.sleepInIgnoredSync(2000);
        p.helper.register();
        p.helper.sleepInIgnoredSync(2000);
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page after submit with alert error message", function() {
        p.helper.get( p.helper.urls.register );
        browser.sleep(500);
        p.helper.fillRegisterForm(null, null, p.helper.userEmailViewer);
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page on account activation success", function() {
        p.helper.sleepInIgnoredSync(2000);
        p.helper.createUser();
        p.helper.sleepInIgnoredSync(2000);
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page on account activation error", function() {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.helper.getEmailedLink(userEmail, p.helper.emailSubjects.register, 'activate').then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.get(url);

            p.loginButton.click();
            p.helper.loginFromCurrPage();
        });
    });

    it("works at restore password page with email input - before submit", function() {
        p.helper.logout();
        p.helper.get(p.helper.urls.restore_password);
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at restore password page with email input - after submit error", function() {
        p.helper.logout();
        p.helper.get(p.helper.urls.restore_password);
        p.helper.forms.restorePassEmail.emailInput.clear().sendKeys(p.helper.userEmailWrong);
        p.helper.forms.restorePassEmail.submitButton.click();
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at restore password page with email input - after submit success", function() {
        p.helper.logout();
        p.helper.get(p.helper.urls.restore_password);
        p.helper.forms.restorePassEmail.emailInput.clear().sendKeys(p.helper.userEmail);
        p.helper.forms.restorePassEmail.submitButton.click();
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at restore password page with password input - before submit", function() {
        var email = p.helper.userEmail;
        p.helper.logout();
        p.helper.forms.restorePassEmail.sendLinkToEmail(email);
        p.helper.getEmailedLink(email, p.helper.emailSubjects.restorePass, 'restore_password').then(function(url) {
            p.helper.get(url);
            browser.sleep(1000);
            p.loginButton.click();
            p.helper.loginFromCurrPage();
        });
    });

    it("works at restore password page with password input - after submit error", function() {
        var email = p.helper.userEmail;
        p.helper.logout();
        p.helper.forms.restorePassEmail.sendLinkToEmail(email);
        p.helper.getEmailedLink(email, p.helper.emailSubjects.restorePass, 'restore_password').then(function(url) {
            p.helper.get(url);
            p.helper.forms.restorePassPassword.setNewPassword(p.helper.userPassword);
            p.helper.get(url);
            p.helper.forms.restorePassPassword.passwordInput.sendKeys(p.helper.userPassword);
            p.helper.forms.restorePassPassword.submitButton.click();
            browser.sleep(2000);
            p.loginButton.click();
            p.helper.loginFromCurrPage();
        });
    });

    it("works at restore password page with password input - after submit success", function() {
        p.helper.logout();
        p.helper.restorePassword();
        p.loginButton.click();
        p.helper.loginFromCurrPage();
        p.helper.logout();
    });
});