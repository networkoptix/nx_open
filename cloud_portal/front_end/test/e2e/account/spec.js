'use strict';
var AccountPage = require('./po.js');
describe('Account suite', function () {

    var p = new AccountPage();

    beforeAll(function() {
        console.log('Account start');
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    afterAll(function() {
        console.log('Account finish');
        p.helper.logout();
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        p.get(p.accountUrl);
        p.alert.submitButton.click();
        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success, true);

    it("should display dropdown in right top corner: Account settings, Change password, Logout", function () {
        p.get(p.homePageUrl);
        expect(p.userAccountDropdownToggle.getText()).toContain(p.helper.userEmail);

        p.userAccountDropdownToggle.click();
        expect(p.userAccountDropdownMenu.getText()).toContain('Account Settings');
        expect(p.userAccountDropdownMenu.getText()).toContain('Change Password');
        expect(p.userAccountDropdownMenu.getText()).toContain('Logout');
    });

    it("should allow to log out", function () {
        p.get(p.homePageUrl);
        p.helper.logout();
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    it("should show email, first name, last name, subscribe", function () {
        p.get(p.accountUrl);

        expect(p.emailField.getAttribute('value')).toContain(p.helper.userEmail);
        expect(p.firstNameInput.isDisplayed()).toBe(true);
        expect(p.lastNameInput.isDisplayed()).toBe(true);
        expect(p.subscribeCheckbox.isDisplayed()).toBe(true);
    });

    it("should allow to edit first name, last name, subscribe, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.helper.userLastName);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userFirstName);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userLastName);
    });

    it("should allow cyrillic First and Last names, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.helper.userNameCyrillic);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.helper.userNameCyrillic);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
    });

    it("should allow smile symbols in First and Last name fields, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.helper.userNameSmile);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.helper.userNameSmile);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
    });

    it("should allow hieroglyphic symbols in First and Last name fields, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.helper.userNameHierog);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.helper.userNameHierog);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
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
        p.get(p.passwordUrl);
        p.prepareToPasswordCheck();
        deferred.fulfill();
        return deferred.promise;
    }, p);

    it("should allow to change password (and change it back)", function () {
        p.get(p.passwordUrl);

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
        p.get(p.passwordUrl);
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

        p.get(p.passwordUrl);

        p.currentPasswordInput.sendKeys(p.helper.userPasswordWrong);
        p.passwordInput.sendKeys(p.helper.userPasswordNew);
        p.alert.submitButton.click();

        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.changePassWrongCurrent, p.alert.alertTypes.danger, true);

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();

        p.get(p.passwordUrl);

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
        p.get(p.passwordUrl);

        p.currentPasswordInput.sendKeys(p.helper.userPasswordWrong);
        p.passwordInput.sendKeys(p.helper.userPassword);
        p.submitButton.click();

        p.alert.catchAlert( p.alert.alertMessages.changePassWrongCurrent, p.alert.alertTypes.danger);
    });

    xit("should not allow to change password if new password is the same as old password", function () {
        expect('test').toBe('written');
    });
});