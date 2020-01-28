'use strict';
var AccountPage = require('./po.js');
describe('On change password page,', function () {

    var p = new AccountPage();

    beforeAll(function() {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    afterAll(function() {
        p.helper.logout();
    });

    beforeEach(function() {
        p.get(p.passwordUrl);
    });

    it("url should be /account/password", function () {
        p.get(p.homePageUrl);

        expect(p.userAccountDropdownToggle.isDisplayed()).toBe(true);

        p.userAccountDropdownToggle.click();
        p.changePasswordLink.click();

        expect(browser.getCurrentUrl()).toContain('/account/password');
        expect(p.htmlBody.getText()).toContain('Current password');
    });

    p.passwordField.check(function(){
        var deferred = protractor.promise.defer();
        p.prepareToPasswordCheck();
        deferred.fulfill();
        return deferred.promise;
    }, p);

    it("restore password", function() {
        p.helper.restorePassword(p.helper.userEmail, p.helper.userPassword);
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    it("password can be changed", function () {
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

    it("password is actually changed, so login works with new password", function () {
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

    it("password change is not possible if old password is wrong", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPasswordWrong);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();

        p.alert.catchAlert( p.alert.alertMessages.changePassWrongCurrent, p.alert.alertTypes.danger);
    });

    it("more than 256 symbols can be entered in new password field and then are cut to 256", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPassword);
        p.passwordInput.sendKeys(p.helper.inputLong300);
        p.submitButton.click();
        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);

        p.refresh();
        //Change password back using 0-255 symbols of that new long password
        p.currentPasswordInput.sendKeys(p.helper.inputLongCut);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();
        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);
    });

    it("pressing Enter key saves data", function () {
        p.currentPasswordInput.sendKeys(p.helper.userPassword);
        // Save form using Enter key
        p.passwordInput.sendKeys(p.helper.userPassword).sendKeys(protractor.Key.ENTER);

        p.alert.catchAlert( p.alert.alertMessages.changePassSuccess, p.alert.alertTypes.success);
    });

    it("pressing Tab key moves focus to the next element", function () {
        // Navigate to next field using TAB key
        p.currentPasswordInput.sendKeys(protractor.Key.TAB);
        p.helper.checkElementFocusedBy(p.passwordInput, 'id');
    });

    xit("should not allow to change password if new password is the same as old password", function () {
        expect('functionality').toBe('implemented');
    });
});