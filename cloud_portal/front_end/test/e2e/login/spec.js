'use strict';
var LoginPage = require('./po.js');
describe('Login suite', function () {

    var p = new LoginPage();

    p.alert.checkAlert(function(){
        p.get();
        p.emailInput.sendKeys(p.userEmailWrong);
        p.passwordInput.sendKeys(p.userPassword);
        p.alert.submitButton.click();
    }, p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger, true);

    it("should open login dialog in anonymous state", function () {
        browser.sleep(1000);
        p.get();
        expect((p.dialogLoginButton).isDisplayed()).toBe(true);
    });

    it("should close login dialog after clicking on background", function () {
        p.get();
        expect((p.dialogLoginButton).isDisplayed()).toBe(true);

        p.loginDialogBackground.click(); // click on login dialog overlay to close it
        expect((p.loginDialog).isPresent()).toBe(false);
    });

    it("should allow to log in with existing credentials and to log out", function () {

        browser.ignoreSynchronization = false;
        p.get();
        p.emailInput.sendKeys(p.userEmail1);
        p.passwordInput.sendKeys(p.userPassword);
        p.login();
        p.logout();
    });

    it("should go to Systems after login; then log out", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1);
        p.passwordInput.sendKeys(p.userPassword);
        p.login();

        // Check that element that user is on page Systems
        expect(browser.getCurrentUrl()).toContain('systems');
        expect(p.htmlBody.getText()).toContain('Systems');

        p.logout();
    });

    it("should show user's email and menu in top right corner; then log out", function () {
        p.get();

        var email = p.userEmail1;

        p.emailInput.sendKeys(email);
        p.passwordInput.sendKeys(p.userPassword);
        p.login();

        expect(p.userAccountDropdownToggle.getText()).toContain(email);

        p.userAccountDropdownToggle.click();
        expect(p.userAccountDropdownMenu.getText()).toContain('Account Settings');
        expect(p.userAccountDropdownMenu.getText()).toContain('Change Password');
        expect(p.userAccountDropdownMenu.getText()).toContain('Logout');
        p.userAccountDropdownToggle.click();

        p.logout();
    });

    it("should not log in with wrong email", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmailWrong);
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.dialogCloseButton.click();
    });

    it("should not allow to log in with existing email in uppercase", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1.toUpperCase());
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);
        p.dialogCloseButton.click();
    });

    it("should not log in with wrong password", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1);
        p.passwordInput.sendKeys(p.userPasswordWrong);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.dialogCloseButton.click();
    });

    it("should not log in without password", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1);
        p.dialogLoginButton.click();

        p.checkPasswordMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should not log in without email but with password", function () {
        p.get();

        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        p.checkEmailMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should not log in without both email and password", function () {
        p.get();

        p.dialogLoginButton.click();

        p.checkPasswordMissing();
        p.checkEmailMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should not log in with email in non-email format but with password", function () {
        p.get();

        p.emailInput.sendKeys('vert546 464w6345');
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        p.checkEmailInvalid();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should show red outline if field is wrong/empty after blur", function () {
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

    it("should log in with Remember Me checkmark switched off", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail2);
        p.passwordInput.sendKeys(p.userPassword);

        p.rememberCheckbox.click(); // Switch off Remember me functionality
        expect(p.rememberCheckbox.isSelected()).toBe(false); // verify that it is really switched off
        expect(p.rememberCheckbox.getAttribute('class')).toContain('ng-empty'); // verify that it is really switched off
        expect(p.rememberCheckbox.getAttribute('class')).not.toContain('ng-not-empty'); // verify that it is really really switched off

        p.login();
        p.logout();
    });

    it("should test I forgot password link", function () {
        p.get();

        var currentEmail = p.userEmail1;
        p.emailInput.sendKeys(currentEmail);
        p.passwordInput.sendKeys(p.userPasswordWrong);
        p.dialogLoginButton.click();
        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Restore password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("should test I forgot password link without attempt to log in first", function () {
        p.get();

        var currentEmail = p.userEmail2; // keep it different from previous case "should test I forgot password link" !!!
        p.emailInput.sendKeys(currentEmail);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Restore password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("should restore password with a code sent to an email", function () {
        var userEmail = p.userEmail1;
        var userPassword = p.userPassword;
        p.get();
        p.iForgotPasswordLink.click();
        p.restoreEmailInput.sendKeys(userEmail);
        p.submitButton.click();

        p.alert.catchAlert( p.alert.alertMessages.restorePassConfirmSent, p.alert.alertTypes.success);

        browser.controlFlow().wait(p.helper.getEmailTo(userEmail).then(function (email) {
            expect(email.subject).toContain("Restore your password");
            expect(email.headers.to).toEqual(userEmail);

            // extract registration token from the link in the email message
            var pattern = /\/static\/index.html#\/restore_password\/([\w=]+)/g;
            var regCode = pattern.exec(email.html)[1];
            console.log(regCode);
            browser.get('/#/restore_password/' + regCode);

            expect(p.newPasswordInput.isPresent()).toBe(true);
            p.newPasswordInput.sendKeys(userPassword); //should be new password here, but old one will do the job too
            p.savePasswordButton.click();
            p.alert.catchAlert( p.alert.alertMessages.restorePassSuccess, p.alert.alertTypes.success);
            browser.refresh();

            p.newPasswordInput.sendKeys(userPassword);
            p.savePasswordButton.click();
            p.alert.catchAlert( p.alert.alertMessages.restorePassWrongCode, p.alert.alertTypes.danger);

            p.helper.login(userEmail, userPassword);
            p.helper.logout();
        }));
    });

    xit("should not log in and show error for inactivated user", function () {
        expect("test").toBe("written");
    });

    xit("should log in with Remember Me checkmark switched on, close browser, open browser and enter same session", function () {
        //TODO: check remember me function by closing browser instance and opening it again
        expect("test").toBe("written");
    });

    xit("should display password masked", function () {
        expect("test").toBe("written with screenshot");
    });

    xit("should copy password masked", function () {
        expect("test").toBe("written with screenshot");
    });

    xit("should lock account after several failed attempts", function () {
        expect("test").toBe("written");
    });

    xit("should not log in, if the user just logged out and pressed back button in browser", function () {
        expect("test").toBe("written");
    });

});