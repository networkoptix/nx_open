'use strict';
var LoginPage = require('./po.js');
describe('Login dialog', function () {

    var p = new LoginPage();

    beforeEach(function() {
        p.get();

        // Log out if logged in
        p.helper.checkPresent(p.loginButton).then( null, function() {
            p.helper.logout();
        });
        p.loginButton.click();
    });

    it("can be opened in anonymous state", function () {
        expect((p.dialogLoginButton).isDisplayed()).toBe(true);
    });

    it("can be closed after clicking on background", function () {
        expect(p.dialogLoginButton.isDisplayed()).toBe(true);

        p.loginDialogBackground.click(); // click on login dialog overlay to close it
        expect((p.loginDialog).isPresent()).toBe(false);
    });

    it("allows to log in with existing credentials and to log out", function () {
        p.emailInput.sendKeys(p.helper.userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.login();
        p.helper.logout();
    });

    it("redirects to Systems after login", function () {
        p.helper.login();
        // No need to check for login success since it's done at login() func
        p.helper.logout();
    });

    it("; after login, display user's email and menu in top right corner", function () {
        var email = p.helper.userEmail;

        p.emailInput.sendKeys(email);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.login();
        p.helper.forms.logout.dropdownToggle.click();

        expect(p.helper.forms.logout.dropdownParent.getText()).toContain(email);
        expect(p.helper.forms.logout.dropdownMenu.getText()).toContain('Account Settings');
        expect(p.helper.forms.logout.dropdownMenu.getText()).toContain('Change Password');
        expect(p.helper.forms.logout.dropdownMenu.getText()).toContain('Log Out');
        p.helper.forms.logout.dropdownToggle.click();

        p.helper.logout();
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.alert.submitButton.click();

        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger, true);

    it("rejects log in with wrong email", function () {
        p.emailInput.sendKeys(p.helper.userEmailWrong);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.dialogCloseButton.click();
    });

    it("allows log in with existing email in uppercase", function () {
        p.emailInput.sendKeys(p.helper.userEmail.toUpperCase());
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.helper.sleepInIgnoredSync(2000);
        p.helper.logout();
    });

    it("rejects log in with wrong password", function () {
        p.emailInput.sendKeys(p.helper.userEmail);
        p.passwordInput.sendKeys(p.helper.userPasswordWrong);
        p.dialogLoginButton.click();

        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.dialogCloseButton.click();
    });

    it("rejects log in without password", function () {
        p.emailInput.sendKeys(p.helper.userEmail);
        p.dialogLoginButton.click();

        p.checkPasswordMissing();

        p.dialogCloseButton.click();
        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("rejects log in without email but with password", function () {
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.checkEmailMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("rejects log in without both email and password", function () {
        p.dialogLoginButton.click();

        p.checkPasswordMissing();
        p.checkEmailMissing();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("rejects log in with email in non-email format but with password", function () {
        p.emailInput.sendKeys('vert546 464w6345');
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();

        p.checkEmailInvalid();

        p.dialogCloseButton.click();

        // Check that element that is visible only for authorized user is NOT displayed on page
        expect(p.loginSuccessElement.isDisplayed()).toBe(false);
    });

    it("shows red outline if field is wrong/empty after blur", function () {
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
        p.emailInput.sendKeys(p.helper.userEmail2);
        p.passwordInput.sendKeys(p.helper.userPassword);

        p.rememberCheckbox.click(); // Switch off Remember me functionality
        expect(p.rememberCheckbox.isSelected()).toBe(false); // verify that it is really switched off
        expect(p.rememberCheckbox.getAttribute('class')).toContain('ng-empty'); // verify that it is really switched off
        expect(p.rememberCheckbox.getAttribute('class')).not.toContain('ng-not-empty'); // verify that it is really really switched off

        p.login();
        p.helper.logout();
    });

    it("contains \'I forgot password\' link that leads to Restore Password page with pre-filled email from login form", function () {
        var currentEmail = p.helper.userEmail;
        p.emailInput.sendKeys(currentEmail);
        p.passwordInput.sendKeys(p.helper.userPasswordWrong);
        p.dialogLoginButton.click();
        p.alert.catchAlert(p.alert.alertMessages.loginIncorrect, p.alert.alertTypes.danger);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Reset Password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("passes email from email input to Restore password page, even without clicking \'Log in\' button", function () {
        var currentEmail = p.helper.userEmail2; // keep it different from previous case "should test I forgot password link" !!!
        p.emailInput.sendKeys(currentEmail);

        p.iForgotPasswordLink.click();

        expect(p.htmlBody.getText()).toContain('Reset Password');
        expect(browser.getCurrentUrl()).toContain('restore_password');

        expect(p.restoreEmailInput.getAttribute('value')).toContain(currentEmail);
    });

    it("rejects log in and shows error for inactivated user", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.loginButton.click();
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.dialogLoginButton.click();
        p.alert.catchAlert(p.alert.alertMessages.loginNotActive, p.alert.alertTypes.danger);
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.loginButton.click();
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.alert.submitButton.click();

        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.loginNotActive, p.alert.alertTypes.danger, true);

    xit("logs in with Remember Me checkmark switched on; after close browser, open browser enters same session", function () {
        //check remember me function by closing browser instance and opening it again
    });

    it("displays password masked", function () {
        p.passwordInput.sendKeys(p.helper.userPassword);
        expect(p.passwordInput.getText()).not.toContain(p.helper.userPassword);
        //browser.takeScreenshot().then(function (png) {
        //    p.helper.writeScreenShot(png, '...../masked_password.png');
        //});
    });

    it("requires login, if the user has just logged out and pressed back button in browser", function () {
        p.helper.login();
        p.helper.logout();
        browser.navigate().back();
        p.helper.sleepInIgnoredSync(2000);
        expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false);
        expect(p.loginDialog.isDisplayed()).toBe(true);
    });

    it("handles more than 256 symbols email and password", function() {
        p.emailInput.clear().sendKeys(p.helper.inputLong300);
        p.passwordInput.clear().sendKeys(p.helper.inputLong300);
        p.dialogLoginButton.click();
        expect(p.emailInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
        expect(p.passwordInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
    });

    it(": logout refreshes page", function() {
        p.helper.login();
        p.helper.get(p.helper.urls.account);
        var accountElem = p.helper.forms.account.lastNameInput;
        p.helper.sleepInIgnoredSync(1000);
        expect(accountElem.isDisplayed()).toBe(true);
        p.helper.logout();
        p.helper.sleepInIgnoredSync(1000);
        browser.sleep(1000);
        expect(accountElem.isPresent()).toBe(false);
    });

    it("allows copy-paste in input fields", function() {
        p.emailInput.sendKeys('copiedValue');

        // Copy + paste
        p.emailInput.sendKeys(protractor.Key.CONTROL, "a")
            .sendKeys(protractor.Key.CONTROL, "c")
            .clear()
            .sendKeys(protractor.Key.CONTROL, "v");
        expect(p.emailInput.getAttribute('value')).toContain('copiedValue');

        p.passwordInput.clear()
            .sendKeys(protractor.Key.CONTROL, "v")
            .sendKeys('123');
        expect(p.passwordInput.getAttribute('value')).toContain('copiedValue123');
        p.emailInput.clear();
        p.passwordInput.clear();

        // Cut + paste
        p.emailInput.sendKeys(protractor.Key.CONTROL, "a")
            .sendKeys(protractor.Key.CONTROL, "x");
        expect(p.emailInput.getAttribute('value')).not.toContain('copiedValue');
        p.emailInput.sendKeys(protractor.Key.CONTROL, "v");
        expect(p.emailInput.getAttribute('value')).toContain('copiedValue');

        p.passwordInput.clear()
            .sendKeys(protractor.Key.chord(protractor.Key.CONTROL, "v"))
            .sendKeys('123');
        expect(p.passwordInput.getAttribute('value')).toContain('copiedValue123');
    });

    it("should respond to Esc key and close dialog", function () {
        p.emailInput.sendKeys(protractor.Key.ESCAPE);
        expect(p.loginDialog.isPresent()).toBe(false);
    });

    it("should respond to Enter key and log in", function () {
        p.emailInput.sendKeys(p.helper.userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword)
            .sendKeys(protractor.Key.ENTER);
        p.helper.sleepInIgnoredSync(2000);
        expect(p.helper.loginSuccessElement.isPresent()).toBe(true);
        p.helper.logout();
    });

    it("should respond to Tab key", function () {
        // Navigate to next field using TAB key
        p.helper.sleepInIgnoredSync(2000);
        browser.sleep(1000);
        p.emailInput.sendKeys(protractor.Key.TAB);
        p.helper.checkElementFocusedBy(p.passwordInput, 'id');
    });

    // Disabled because current chromedriver does not support it
    xit("should respond to Space key and toggle checkbox", function () {
        p.passwordInput.sendKeys(protractor.Key.TAB);
        p.rememberCheckbox.sendKeys(protractor.Key.SPACE);
        expect(p.rememberCheckbox.isSelected()).toBe(false); // verify that it is switched off
        expect(p.rememberCheckbox.getAttribute('class')).toContain('ng-empty'); // verify that it is really switched off

        p.rememberCheckbox.sendKeys(protractor.Key.SPACE);
        expect(p.rememberCheckbox.isSelected()).toBe(true); // verify that it is switched on
        expect(p.rememberCheckbox.getAttribute('class')).not.toContain('ng-empty'); // verify that it is really switched on
    });

    it("handles two tabs, updates second tab state if logout is done on first", function() {
        var termsConditions = element(by.linkText('Terms and Conditions'));
        // Open Terms and Conditions link allows to open new tab. Opening new browser Tab is not supported in Selenium
        p.helper.get(p.helper.urls.register);
        termsConditions.click();
        p.helper.get(); // moving away from register page (because it logs out)

        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindow = handles[0];
            var newWindow = handles[1];
            browser.switchTo().window(newWindow).then(function () {
                browser.refresh();
                p.helper.sleepInIgnoredSync(1000);
                expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false); // user is logged out
                p.helper.login();
                p.helper.sleepInIgnoredSync(1000);
                expect(p.helper.loginSuccessElement.isDisplayed()).toBe(true); // user is logged in
            });
            browser.switchTo().window(oldWindow).then(function () {
                browser.refresh();
                p.helper.sleepInIgnoredSync(1000);
                expect(p.helper.loginSuccessElement.isDisplayed()).toBe(true); // user is logged in
                p.helper.logout();
                p.helper.sleepInIgnoredSync(1000);
                expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false); // user is logged out
            });
            browser.switchTo().window(newWindow).then(function () {
                browser.refresh();
                p.helper.sleepInIgnoredSync(1000);
                expect(p.helper.loginSuccessElement.isDisplayed()).toBe(false); // user is logged out
                browser.close();
            });
            browser.switchTo().window(oldWindow);
        });
    });

    it("handles two tabs, rejects navigation on second tab if logout is done on first", function() {
        var termsConditions = element(by.linkText('Terms and Conditions'));
        // Open Terms and Conditions link allows to open new tab. Opening new browser Tab is not supported in Selenium
        p.helper.get(p.helper.urls.register);
        browser.sleep(2000);
        termsConditions.click();
        p.helper.get(); // moving away from register page (because it logs out)
        p.helper.login();

        // Switch to the new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindow = handles[0];
            var newWindow = handles[1];
            browser.switchTo().window(newWindow).then(function () {
                // new tab should be refreshed automatically after user logs in at the old tab
                browser.refresh();
                // In real world it works correctly without refresh, but browser.switchTo() does not perform
                // a real switch between tabs. For browser, tab stays not active,
                // so login there does not affect other tabs
                p.helper.forms.logout.dropdownToggle.click();
                expect(p.helper.forms.logout.dropdownParent.getText()).toContain('noptixqa'); // user is logged in
                p.helper.forms.logout.dropdownToggle.click();
                p.helper.logout();
            });
            browser.switchTo().window(oldWindow).then(function () {
                expect(p.helper.forms.logout.dropdownToggle.isPresent()).toBe(true);
                browser.refresh();
                // In real world it works correctly without refresh, but browser.switchTo() does not perform
                // a real switch between tabs. For browser, tab stays not active,
                // so logout there does not affect other tabs
                expect(p.loginDialog.isPresent()).toBe(true);
            });
        });
    });
});
