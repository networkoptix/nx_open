'use strict';
var RegisterPage = require('./po.js');
describe('Registration step2', function () {
    var p = new RegisterPage();

    beforeEach(function() {
        p.helper.get(p.url);
    });

    xit("should open register page from register success page by clicking Register button on top right corner", function () {
        p.helper.register();

        p.openRegisterButton.click();

        expect(browser.getCurrentUrl()).toContain('register');
        expect(p.htmlBody.getText()).toContain('Welcome to Nx Cloud');
    });

    it("should activate registration with a registration code sent to an email", function () {
        var userEmail = p.helper.register();

        p.getActivationPage(userEmail).then( function() {
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);

            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.logout();
        });
    });

    it("should save user data to user account correctly", function () {
        var userEmail = p.helper.register();
    
        p.getActivationPage(userEmail).then( function() {
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.get('/#/account');
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
    
        p.getActivationPage(userEmail).then( function() {
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.get('/#/account');
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
    
        p.getActivationPage(userEmail).then( function() {
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);
            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.get('/#/account');
            expect(p.accountFirstNameInput.getAttribute('value')).toMatch(p.helper.userFirstName);
            expect(p.accountLastNameInput.getAttribute('value')).toMatch(p.helper.userLastName);
            p.helper.logout();
        });
    });
});