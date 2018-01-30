'use strict';
var LoginPage = require('./po.js');
describe('Login with incorrect credentials', function () {

    var p = new LoginPage();

    it("works at registration page before submit", function() {
        p.helper.get( p.helper.urls.register );
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
    });

    it("works at registration page after submit success", function() {
        p.helper.register();
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
    });

    it("works at registration page after submit with alert error message", function() {
        p.helper.get( p.helper.urls.register );
        p.helper.fillRegisterForm(null, null, p.helper.userEmailViewer);
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
    });

    it("works at registration page on account activation success", function() {
        p.helper.createUser();
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
    });

    it("works at registration page on account activation error", function() {
        var userEmail = p.helper.getRandomEmail();
 
        p.helper.register(null, null, userEmail);
        p.helper.getEmailedLink(userEmail, p.helper.emailSubjects.register, 'activate').then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.get(url);

            p.loginButton.click();
            p.helper.loginFromCurrPage(p.helper.userEmailWrong);
        });
    });

    it("works at restore password page with email input - before submit", function() {
        p.helper.logout();
        p.helper.get(p.helper.urls.restore_password);
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
    });

    it("works at restore password page with email input - after submit error", function() {
        p.helper.logout();
        p.helper.get(p.helper.urls.restore_password);
        p.helper.forms.restorePassEmail.emailInput.clear().sendKeys(p.helper.userEmailWrong);
        p.helper.forms.restorePassEmail.submitButton.click();
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
    });

    it("works at restore password page with email input - after submit success", function() {
        p.helper.logout();
        p.helper.get(p.helper.urls.restore_password);
        p.helper.forms.restorePassEmail.emailInput.clear().sendKeys(p.helper.userEmail);
        p.helper.forms.restorePassEmail.submitButton.click();
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
    });

    it("works at restore password page with password input - before submit", function() {
        var email = p.helper.userEmailAdmin;
        p.helper.logout();
        p.helper.forms.restorePassEmail.sendLinkToEmail(email);
        p.helper.getEmailedLink(email, p.helper.emailSubjects.restorePass, 'restore_password').then(function(url) {
            p.helper.get(url);

            p.loginButton.click();
            p.helper.loginFromCurrPage(p.helper.userEmailWrong);
        });
    });

    it("works at restore password page with password input - after submit error", function() {
        var email = p.helper.userEmailViewer;
        p.helper.logout();
        p.helper.forms.restorePassEmail.sendLinkToEmail(email);
        p.helper.getEmailedLink(email, p.helper.emailSubjects.restorePass, 'restore_password').then(function(url) {
            p.helper.get(url);
            p.helper.forms.restorePassPassword.setNewPassword(p.helper.userPassword);
            p.helper.get(url);
            p.helper.forms.restorePassPassword.passwordInput.sendKeys(p.helper.userPassword);
            p.helper.forms.restorePassPassword.submitButton.click();

            p.loginButton.click();
            p.helper.loginFromCurrPage(p.helper.userEmailWrong);
        });
    });

    it("works at restore password page with password input - after submit success", function() {
        p.helper.logout();
        p.helper.restorePassword();
        p.loginButton.click();
        p.helper.loginFromCurrPage(p.helper.userEmailWrong);
        p.helper.logout();
    });
});