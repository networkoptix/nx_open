'use strict';
var LoginPage = require('./po.js');
describe('Login dialog', function () {

    var p = new LoginPage();

    it("can be opened in anonymous state", function () {
        browser.sleep(1000);
        p.get();
        expect((p.dialogLoginButton).isDisplayed()).toBe(true);
    });

    it("can be closed after clicking on background", function () {
        p.get();
        expect((p.dialogLoginButton).isDisplayed()).toBe(true);

        p.loginDialogBackground.click(); // click on login dialog overlay to close it
        expect((p.loginDialog).isPresent()).toBe(false);
    });

    it("allows to log in with existing credentials and to log out", function () {

        browser.ignoreSynchronization = false;
        p.get();
        p.emailInput.sendKeys(p.helper.userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.login();
        p.logout();
    });

    it("redirects to Systems after login", function () {
        p.get();

        p.emailInput.sendKeys(p.helper.userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.login();

        // Check that element that user is on page Systems
        expect(browser.getCurrentUrl()).toContain('systems');
        expect(p.htmlBody.getText()).toContain('Systems');

        p.logout();
    });

    it("; after login, display user's email and menu in top right corner", function () {
        p.get();

        var email = p.helper.userEmail;

        p.emailInput.sendKeys(email);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.login();

        expect(p.userAccountDropdownToggle.getText()).toContain(email);

        p.userAccountDropdownToggle.click();
        expect(p.userAccountDropdownMenu.getText()).toContain('Account Settings');
        expect(p.userAccountDropdownMenu.getText()).toContain('Change Password');
        expect(p.userAccountDropdownMenu.getText()).toContain('Logout');
        p.userAccountDropdownToggle.click();

        p.logout();
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        p.get();
        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.alert.submitButton.click();

        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger, true);

    it("rejects log in with wrong email", function () {
        p.get();

        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.dialogCloseButton.click();
    });

    it("rejects log in with existing email in uppercase", function () {
        p.get();

        p.emailInput.sendKeys(p.helper.userEmail.toUpperCase());
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);
        p.dialogCloseButton.click();
    });

    it("rejects log in with wrong password", function () {
        p.get();

        p.emailInput.sendKeys(p.helper.userEmail);
        p.passwordInput.sendKeys(p.helper.userPasswordWrong);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.dialogCloseButton.click();
    });

    it("rejects log in without password", function () {
        p.get();

        p.emailInput.sendKeys(p.helper.userEmail);
        p.dialogLoginButton.click();

        p.checkPasswordMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("rejects log in without email but with password", function () {
        p.get();

        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.checkEmailMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("rejects log in without both email and password", function () {
        p.get();

        p.dialogLoginButton.click();

        p.checkPasswordMissing();
        p.checkEmailMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("rejects log in with email in non-email format but with password", function () {
        p.get();

        p.emailInput.sendKeys('vert546 464w6345');
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.checkEmailInvalid();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("shows red outline if field is wrong/empty after blur", function () {
        p.get();

        p.emailInput.sendKeys('vert546 464w6345');
        p.rememberCheckbox.click(); // blur email field
        p.checkEmailInvalid();
        expect(p.passwordInputWrap.getAttribute('class')).not.toContain('has-error'); // since password is not touched, field shoud show no error


        p.passwordInput.click(); // click on password and leave it empty
        p.rememberCheckbox.click(); // blur password field
        p.checkPasswordMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("allows log in with \'Remember Me checkmark\' switched off", function () {
        p.get();

        p.emailInput.sendKeys(p.helper.userEmail2);
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.rememberCheckbox.click(); // Switch off Remember me functionality
        expect(p.rememberCheckbox.isSelected()).toBe(false); // verify that it is really switched off
        expect(p.rememberCheckbox.getAttribute('class')).toContain('ng-empty'); // verify that it is really switched off
        expect(p.rememberCheckbox.getAttribute('class')).not.toContain('ng-not-empty'); // verify that it is really really switched off

        p.login();
        p.logout();
    });

    it("contains \'I forgot password\' link that leads to Restore Password page with pre-filled email from login form", function () {
        p.get();

        var currentEmail = p.helper.userEmail;
        p.emailInput.sendKeys(currentEmail);
        p.passwordInput.sendKeys(p.helper.userPasswordWrong);
        p.dialogLoginButton.click();
        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Restore password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("passes email from email input to Restore password page, even without clicking \'Log in\' button", function () {
        p.get();

        var currentEmail = p.helper.userEmail2; // keep it different from previous case "should test I forgot password link" !!!
        p.emailInput.sendKeys(currentEmail);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Restore password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("rejects log in and shows error for inactivated user", function () {
        var userEmail = p.helper.register();

        p.get();
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();
        p.alert.catchAlert(p.alert.alertMessages.loginNotActive, p.alert.alertTypes.danger);
        browser.pause();
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        var userEmail = p.helper.register();

        p.get();
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.alert.submitButton.click();

        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.loginNotActive, p.alert.alertTypes.danger, true);

    xit("logs in with Remember Me checkmark switched on; after close browser, open browser enters same session", function () {
        //check remember me function by closing browser instance and opening it again
        expect("test").toBe("written");
    });

    xit("displays password masked", function () {
        expect("test").toBe("written with screenshot");
    });

    xit("allows to copy password masked", function () {
        expect("test").toBe("written with screenshot");
    });

    xit("locks account after several failed attempts", function () {
        expect("functionality").toBe("implemented");
    });

    xit("rejects log in, if the user just logged out and pressed back button in browser", function () {
        expect("test").toBe("written");
    });

    xit("handles more than 256 symbols email and password", function() {
        /*
         Use email with 256 symbols
         Use email with more than 256 symbols
         Use password with 256 symbols
         Use password with much more than 256 symbols
         */
    });
    xit(": logout refreshes page", function() {
    });
    xit("can be opened in new tab", function() {
    });
    xit("allows copy-paste in input fields", function() {
    });
    xit("supports hotkeys", function() {
    });
    xit("handles two tabs, updates second tab state if logout is done on first", function() {
    });
    it("works at registration page before submit - with correct credentials", function() {
        p.helper.get( p.helper.urls.register );
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page after submit success - with correct credentials", function() {
        p.helper.register();
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page after submit with alert error message - with correct credentials", function() {
        p.helper.register(null, null, p.helper.userEmailViewer);
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page on account activation success - with correct credentials", function() {
        p.helper.createUser();
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    it("works at registration page on account activation error - with correct credentials", function() {
        var userEmail = p.helper.register();

        p.helper.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.get(url);

            p.loginButton.click();
            p.helper.loginFromCurrPage();
        });
    });

    xit("works at restore password page with email input - before submit - with correct credentials", function() {

    });

    xit("works at restore password page with email input - after submit error - with correct credentials", function() {

    });

    xit("works at restore password page with email input - after submit success - with correct credentials", function() {

    });

    xit("works at restore password page with password input - before submit - with correct credentials", function() {
        p.helper.register(null, null, p.helper.userEmailViewer);
        p.loginButton.click();
        p.helper.loginFromCurrPage();
    });

    xit("works at restore password page with password input - after submit error - with correct credentials", function() {

    });

    xit("works at restore password page with password input - after submit success - with correct credentials", function() {

    });

    xit("works at each possible page at each state - with incorrect credentials", function() {
        /*
         Homepage

         Registration page - before submit
         Registration page - after submit success
         Registration page - after submit with alert error message
         Registration page - on account activation success
         Registration page - on account activation error

         Restore password page with email input - before submit
         Restore password page with email input - after submit error
         Restore password page with email input - after submit success
         Restore password page with password input - before submit
         Restore password page with password input - after submit error
         Restore password page with password input - after submit success
         */
    });
});