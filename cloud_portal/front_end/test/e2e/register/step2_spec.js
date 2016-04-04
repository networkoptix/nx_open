'use strict';
var RegisterPage = require('./po.js');
describe('Registration step2', function () {
    var p = new RegisterPage();

    beforeAll(function() {
        console.log('Registration step2 start');
    });

    afterAll(function() {
        console.log('Registration step2 finish');
    });

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
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success);
            browser.refresh();
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmError, p.alert.alertTypes.danger);

            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.logout();
        });
    });

    it("should save user data to user account correctly", function () {
        var userEmail = p.helper.register();

        p.getActivationPage(userEmail).then( function() {
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success);
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
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success);
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
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success);
            p.helper.login(userEmail, p.helper.userPassword);
            p.helper.get('/#/account');
            expect(p.accountFirstNameInput.getAttribute('value')).toMatch(p.helper.userFirstName);
            expect(p.accountLastNameInput.getAttribute('value')).toMatch(p.helper.userLastName);
            p.helper.logout();
        });
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        var userEmail = p.helper.register();

        p.getActivationPage(userEmail).then( function() {
            deferred.fulfill();
        });

        return deferred.promise;
    }, p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success, false);

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        var userEmail = p.helper.register();

        p.getActivationPage(userEmail).then( function() {
            p.alert.catchAlert( p.alert.alertMessages.registerConfirmSuccess, p.alert.alertTypes.success);
            browser.refresh();

            deferred.fulfill();
        });

        return deferred.promise;
    }, p.alert.alertMessages.registerConfirmError, p.alert.alertTypes.danger, false);
});