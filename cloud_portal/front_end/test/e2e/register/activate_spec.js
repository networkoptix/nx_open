'use strict';
var RegisterPage = require('./po.js');
describe('User activation', function () {
    var p = new RegisterPage();

    beforeEach(function() {
        p.helper.get(p.url);
    });

    it("should open register page from register success page by clicking Register button on top right corner", function () {
        p.helper.register();

        p.openRegisterButton.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.helper.htmlBody.getText()).toContain('By clicking Register, you agree to our Terms and Conditions');
    });

    it("should activate registration with a registration code sent to an email", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            expect(browser.getCurrentUrl()).toContain("/activate/success");
            expect(p.messageLoginLink.isDisplayed()).toBe(true);
            p.messageLoginLink.click();
            expect(p.helper.forms.login.dialog.isDisplayed()).toBe(true);
        });
    });

    it("should show error if same link is used twice", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            browser.get(url);
            p.alert.catchAlert(p.alert.alertMessages.registerConfirmError, p.alert.alertTypes.danger);
        });
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            browser.get(url);
            deferred.fulfill();
        });
        return deferred.promise;
    }, p.alert.alertMessages.registerConfirmError, p.alert.alertTypes.danger, true);

    it("should save user data to user account correctly", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.get('/account');
            expect(p.accountFirstNameInput.getAttribute('value')).toMatch(p.helper.userFirstName);
            expect(p.accountLastNameInput.getAttribute('value')).toMatch(p.helper.userLastName);
            p.helper.logout();
        });
    });
    
    it("should allow to enter more than 256 symbols in First and Last names and cut it to 256", function () {
        var userEmail = p.helper.getRandomEmail();
    
        p.firstNameInput.sendKeys(p.helper.inputLong300);
        p.lastNameInput.sendKeys(p.helper.inputLong300);
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
    
        p.submitButton.click();
    
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.get('/account');
            expect(p.accountFirstNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
            expect(p.accountLastNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
            p.helper.logout();
        });
    });
    
    it("should trim leading and trailing spaces", function () {
        var userEmail = p.helper.getRandomEmail();
    
        p.firstNameInput.sendKeys(' ' + p.helper.userFirstName + ' ');
        p.lastNameInput.sendKeys(' ' + p.helper.userLastName + ' ');
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
    
        p.submitButton.click();
    
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.get('/account');
            expect(p.accountFirstNameInput.getAttribute('value')).toMatch(p.helper.userFirstName);
            expect(p.accountLastNameInput.getAttribute('value')).toMatch(p.helper.userLastName);
            p.helper.logout();
        });
    });

    it("should display Open Nx Witness button after activation, if user is registered by link /register/?from=client", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.get(p.url + '?from=client');

        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            expect(p.openInClientButton.isDisplayed()).toBe(true);
        });
    });

    it("should display Open Nx Witness button after activation, if user is registered by link /register/?from=mobile", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.get(p.url + '?from=mobile');

        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.sendKeys(p.helper.userLastName);
        p.emailInput.sendKeys(userEmail);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            expect(p.openInClientButton.isDisplayed()).toBe(true);
        });
    });

    it("link works and logs out user, if he was logged in", function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        p.helper.login(p.helper.userEmail);
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            expect(p.helper.forms.logout.dropdownToggle.isDisplayed()).toBe(false);
            expect(p.messageLoginLink.isDisplayed()).toBe(true);
            expect(p.helper.forms.login.openLink.isDisplayed()).toBe(true);
        });
    });

    it('email can be sent again', function () {
        var userEmail = p.helper.getRandomEmail();

        p.helper.register(null, null, userEmail);
        // Read email but not visit the activation link
        p.getActivationLink(userEmail).then( function(url) { console.log(url) });
        // Login
        p.helper.forms.login.openLink.click();
        p.helper.loginFromCurrPage(userEmail);
        // Follow the link in alert
        //browser.pause();
        browser.ignoreSynchronization = true;
        element(by.linkText('Send confirmation link again')).click();
        browser.ignoreSynchronization = false;
        // Request one more email
        expect(browser.getCurrentUrl()).toContain('activate');
        expect(element(by.css('h1')).getText()).toContain('Confirm your account');
        element(by.buttonText('Send')).click();
        // Check email again and follow the -white rabbit- link
        p.getActivationLink(userEmail).then( function(url) {
            p.helper.get(url);
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
        });
    });

    xit("If no user was logged in, confirmation link logs new user in (not implemented)", function () {
    });
});