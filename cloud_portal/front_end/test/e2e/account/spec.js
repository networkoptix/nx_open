'use strict';
var AccountPage = require('./po.js');
describe('Account suite', function () {

    var p = new AccountPage();

    beforeAll(function() {
        p.helper.login(p.userEmail, p.userPassword);
    });

    afterAll(function() {
        p.helper.logout();
    });

    it("should display dropdown in right top corner: Account settings, Change password, Logout", function () {
        p.get(p.homePageUrl);
        expect(p.userAccountDropdownToggle.getText()).toContain(p.userEmail);

        p.userAccountDropdownToggle.click();
        expect(p.userAccountDropdownMenu.getText()).toContain('Account Settings');
        expect(p.userAccountDropdownMenu.getText()).toContain('Change Password');
        expect(p.userAccountDropdownMenu.getText()).toContain('Logout');
    });

    it("should allow to log out", function () {
        p.get(p.homePageUrl);
        p.helper.logout();
        p.helper.login(p.userEmail, p.userPassword);
    });

    it("should show email, first name, last name, subscribe", function () {
        p.get(p.accountUrl);
        expect(p.emailField.getAttribute('value')).toContain(p.userEmail);
        expect(p.firstNameInput.isDisplayed()).toBe(true);
        expect(p.lastNameInput.isDisplayed()).toBe(true);
        expect(p.subscribeCheckbox.isDisplayed()).toBe(true);
    });

    it("should allow to edit first name, last name, subscribe, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userFirstName);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userLastName);

        p.saveButton.click();

        p.helper.catchAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userFirstName);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userLastName);
    });

    it("should allow cyrillic First and Last names, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userNameCyrillic);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userNameCyrillic);

        p.saveButton.click();

        p.helper.catchAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userNameCyrillic);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userNameCyrillic);
    });

    it("should allow smile symbols in First and Last name fields, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userNameSmile);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userNameSmile);

        p.saveButton.click();

        p.helper.catchAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userNameSmile);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userNameSmile);
    });

    it("should allow hieroglyphic symbols in First and Last name fields, and save it", function () {
        p.get(p.accountUrl);

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userNameHierog);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userNameHierog);

        p.saveButton.click();

        p.helper.catchAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userNameHierog);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userNameHierog);
    });

    it("should open change password page", function () {
        p.get(p.homePageUrl);

        expect(p.userAccountDropdownToggle.isDisplayed()).toBe(true);

        p.userAccountDropdownToggle.click();
        p.changePasswordLink.click();

        expect(browser.getCurrentUrl()).toContain('#/account/password');
        expect(p.htmlBody.getText()).toContain('Current password');
    });

    p.passwordField.check(p, p.passwordUrl);

    it("should allow to change password (and change it back)", function () {
        p.get(p.passwordUrl);

        p.currentPasswordInput.sendKeys(p.userPassword);
        p.passwordInput.sendKeys(p.userPasswordNew);
        p.submitButton.click();

        p.helper.catchAlert(p.passwordChangeAlert, 'Your password was successfully changed.');

        browser.refresh();

        p.currentPasswordInput.sendKeys(p.userPasswordNew);
        p.passwordInput.sendKeys(p.userPassword);
        p.submitButton.click();

        p.helper.catchAlert(p.passwordChangeAlert, 'Your password was successfully changed.');
    });

    it("should save new password correctly (and change it back)", function () {
        p.get(p.passwordUrl);
        p.currentPasswordInput.sendKeys(p.userPassword);
        p.passwordInput.sendKeys(p.userPasswordNew);
        p.submitButton.click();
        p.helper.catchAlert(p.passwordChangeAlert, 'Your password was successfully changed.');

        p.helper.logout();
        p.helper.login(p.userEmail, p.userPasswordNew);

        p.get(p.passwordUrl);
        p.currentPasswordInput.sendKeys(p.userPasswordNew);
        p.passwordInput.sendKeys(p.userPassword);
        p.submitButton.click();
        p.helper.catchAlert(p.passwordChangeAlert, 'Your password was successfully changed.');
    });

    it("should not allow to change password if old password is wrong", function () {
        p.get(p.passwordUrl);

        p.currentPasswordInput.sendKeys(p.userPasswordWrong);
        p.passwordInput.sendKeys(p.userPassword);
        p.submitButton.click();

        p.helper.catchAlert(p.passwordChangeAlert, 'Couldn\'t change your password: Current password doesn\'t match');
    });

    xit("should not allow to change password if new password is the same as old password", function () {
        expect('test').toBe('written');
    });
});