'use strict';
var AccountPage = require('./po.js');
describe('Change password suite', function () {

    var p = new AccountPage();

    beforeAll(function() {
        console.log('\nChange password start\n');
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    afterAll(function() {
        console.log('\nChange password finish\n');
        p.helper.logout();
    });

    beforeEach(function() {
        p.get(p.passwordUrl);
    });

    it("should open change password page", function () {
        p.get(p.homePageUrl);

        expect(p.userAccountDropdownToggle.isDisplayed()).toBe(true);

        p.userAccountDropdownToggle.click();
        p.changePasswordLink.click();

        expect(browser.getCurrentUrl()).toContain('#/account/password');
        expect(p.htmlBody.getText()).toContain('Current password');
    });

    p.passwordField.check(function(){
        var deferred = protractor.promise.defer();
        p.prepareToPasswordCheck();
        deferred.fulfill();
        return deferred.promise;
    }, p);

    it("should allow to change password (and change it back)", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPassword);
        p.passwordInput.sendKeys(p.helper.userPasswordNew);
        p.submitButton.click();

        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);

        browser.refresh();

        p.currentPasswordInput.sendKeys(p.helper.userPasswordNew);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();

        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);
    });

    it("should save new password correctly (and change it back)", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPassword);
        p.passwordInput.sendKeys(p.helper.userPasswordNew);
        p.submitButton.click();
        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);

        p.helper.logout();
        p.helper.login(p.helper.userEmail, p.helper.userPasswordNew);

        p.get(p.passwordUrl);
        p.currentPasswordInput.sendKeys(p.helper.userPasswordNew);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();
        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        p.currentPasswordInput.sendKeys(p.helper.userPasswordWrong);
        p.passwordInput.sendKeys(p.helper.userPasswordNew);
        p.alert.submitButton.click();

        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.changePassWrongCurrent, p.alert.alertTypes.danger, true);

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        p.currentPasswordInput.sendKeys(p.helper.userPassword);
        p.passwordInput.sendKeys(p.helper.userPasswordNew);
        p.alert.submitButton.click();

        p.currentPasswordInput.clear();
        p.currentPasswordInput.sendKeys(p.helper.userPasswordNew);
        p.passwordInput.clear();
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.alert.submitButton.click();

        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success, true);

    it("should not allow to change password if old password is wrong", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPasswordWrong);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();

        p.alert.catchAlert( p.alert.alertMessages.changePassWrongCurrent, p.alert.alertTypes.danger);
    });

    it("should allow to enter more than 256 symbols in new password field; change password back using 0-255 symbols of that new long password", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPassword);
        p.passwordInput.sendKeys(p.helper.inputLong300);
        p.submitButton.click();
        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);

        p.refresh();

        p.currentPasswordInput.sendKeys(p.helper.inputLongCut);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();
        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);
    });

    it("should respond to Enter key and save data", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPassword);
        // Save form using Enter key
        p.passwordInput.sendKeys(p.helper.userPassword).sendKeys(protractor.Key.ENTER);

        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);
    });

    it("should respond to Tab key", function () {
        // Navigate to next field using TAB key
        p.currentPasswordInput.sendKeys(protractor.Key.TAB);
        p.helper.checkElementFocusedBy(p.passwordInput, 'id');
    });

    xit("should not allow to change password if new password is the same as old password", function () {
        expect('test').toBe('written');
    });
});