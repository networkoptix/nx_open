'use strict';
var LoginPage = require('./po.js');
describe('Login suite', function () {

    var p = new LoginPage();

    it("should open login dialog in anonimous state", function () {
        p.get();
        expect((p.dialogLoginButton).isDisplayed()).toBe(true);
    });

    it("should close login dialog after clicking on background", function () {
        p.get();
        expect((p.dialogLoginButton).isDisplayed()).toBe(true);

        p.loginDialogBackground.click(); // click on login dialog background to close it
        expect((p.loginDialog).isPresent()).toBe(false);
    });

    it("should allow to log in with existing credentials and to log out", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1);
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        browser.sleep(2000); // such a shame, but I can't solve it right now
        // Check that element that is visible only for authorized user is displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(true);

        // Log out
        p.userAccountDropdownToggle.click();
        p.userLogoutLink.click();
        browser.sleep(500);

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should go to Systems after login; then log out", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1);
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that user is on page Systems
        expect(browser.getCurrentUrl()).toContain('systems');
        expect(p.htmlBody.getText()).toContain('Systems');

        // Log out
        p.userAccountDropdownToggle.click();
        p.userLogoutLink.click();
        browser.sleep(500);

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should show user's email and menu in top right corner; then log out", function () {
        p.get();

        var email = p.userEmail1;

        p.emailInput.sendKeys(email);
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that user is on page Systems
        expect(p.userAccountDropdownToggle.getText()).toContain(email);

        // Log out
        p.userAccountDropdownToggle.click();
        expect(p.userAccountDropdownMenu.getText()).toContain('Account Settings');
        expect(p.userAccountDropdownMenu.getText()).toContain('Change Password');
        expect(p.userAccountDropdownMenu.getText()).toContain('Logout');
        p.userLogoutLink.click();
        browser.sleep(500);

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should not log in with wrong email", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmailWrong);
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        p.catchLoginIncorrectAlert(p.loginIncorrectAlert);

        p.dialogCloseButton.click();
    });

    it("should not allow to log in with existing email in uppercase", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1.toUpperCase());
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        p.catchLoginIncorrectAlert(p.loginIncorrectAlert);

        p.dialogCloseButton.click();
    });

    it("should not log in with wrong password", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1);
        p.passwordInput.sendKeys(p.userPasswordWrong);
        p.dialogLoginButton.click();

        p.catchLoginIncorrectAlert(p.loginIncorrectAlert);

        p.dialogCloseButton.click();
    });

    it("should not log in without password", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail1);
        p.dialogLoginButton.click();

        expect(p.passwordInput.getAttribute('class')).toContain('ng-invalid-required');

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should not log in without email but with password", function () {
        p.get();

        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        expect(p.emailInput.getAttribute('class')).toContain('ng-invalid-required');

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should not log in without both email and password", function () {
        p.get();

        p.dialogLoginButton.click();

        expect(p.passwordInput.getAttribute('class')).toContain('ng-invalid-required');
        expect(p.emailInput.getAttribute('class')).toContain('ng-invalid-required');

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });


    it("should not log in with email in non-email format but with password", function () {
        p.get();

        p.emailInput.sendKeys('vert546 464w6345');
        p.passwordInput.sendKeys(p.userPassword);
        p.dialogLoginButton.click();

        expect(p.emailInput.getAttribute('class')).toContain('ng-invalid-email');

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should log in with Remember Me checkmark switched off", function () {
        p.get();

        p.emailInput.sendKeys(p.userEmail2);
        p.passwordInput.sendKeys(p.userPassword);

        p.remembermeCheckbox.click(); // Switch off Remember me functionality
        expect(p.remembermeCheckbox.getAttribute('class')).toContain('ng-empty'); // verify that it is really switched off
        expect(p.remembermeCheckbox.getAttribute('class')).not.toContain('ng-not-empty'); // verify that it is really really switched off

        p.dialogLoginButton.click();

        browser.sleep(2000); // such a shame, but I can't solve it right now

        // Check that element that is visible only for authorized user is displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(true);

        // Log out
        p.userAccountDropdownToggle.click();
        p.userLogoutLink.click();
        browser.sleep(500);

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("should test I forgot pasword link", function () {
        p.get();

        var currentEmail = p.userEmail1;
        p.emailInput.sendKeys(currentEmail);
        p.passwordInput.sendKeys(p.userPasswordWrong);
        p.dialogLoginButton.click();
        p.catchLoginIncorrectAlert(p.loginIncorrectAlert);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Restore password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("should test I forgot pasword link without attempt to log in first", function () {
        p.get();

        var currentEmail = p.userEmail2; // keep it different from previous case "should test I forgot pasword link" !!!
        p.emailInput.sendKeys(currentEmail);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Restore password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("should not log in and show error for inactivated user", function () {
        expect("test").toBe("written");
    });

    it("should log in with Remember Me checkmark switched on, close browser, open browser and enter same session", function () {
        //TODO: check remember me function by closing browser instance and opening it again
        expect("test").toBe("written");
    });

    it("should display password masked", function () {
        expect("test").toBe("written with screenshot");
    });

    it("should copy password masked", function () {
        expect("test").toBe("written with screenshot");
    });

    it("should lock account after several failed attempts", function () {
        expect("test").toBe("written");
    });

    it("should not log in, if the user just logged out and pressed back button in browser", function () {
        expect("test").toBe("written");
    });

});