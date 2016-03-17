'use strict';
var AccountPage = require('./po.js');
describe('Account suite', function () {

    var p = new AccountPage();

    beforeAll(function() {
        p.getHomePage();
        p.login();
    });

    it("should display dropdown in right top corner: Account settings, Change password, Logout", function () {
        p.getHomePage();

        expect(p.userAccountDropdownToggle.getText()).toContain(p.userEmail);

        p.userAccountDropdownToggle.click();
        expect(p.userAccountDropdownMenu.getText()).toContain('Account Settings');
        expect(p.userAccountDropdownMenu.getText()).toContain('Change Password');
        expect(p.userAccountDropdownMenu.getText()).toContain('Logout');
    });

    it("should allow to log out", function () {
        p.getHomePage();
        p.logout();
        p.login();
    });

    it("should show email, first name, last name, subscribe", function () {
        p.getByUrl();
        expect(p.emailField.getAttribute('value')).toContain(p.userEmail);
        expect(p.firstNameInput.isDisplayed()).toBe(true);
        expect(p.lastNameInput.isDisplayed()).toBe(true);
        expect(p.subscribeCheckbox.isDisplayed()).toBe(true);
    });

    it("should allow to edit first name, last name, subscribe, and save it", function () {
        p.getByUrl();

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userFirstName);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userLastName);

        p.saveButton.click();

        p.catchSuccessAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userFirstName);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userLastName);
    });

    it("should allow cyrillic First and Last names, and save it", function () {
        p.getByUrl();

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userNameCyrillic);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userNameCyrillic);

        p.saveButton.click();

        p.catchSuccessAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userNameCyrillic);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userNameCyrillic);
    });

    it("should allow smile symbols in First and Last name fields, and save it", function () {
        p.getByUrl();

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userNameSmile);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userNameSmile);

        p.saveButton.click();

        p.catchSuccessAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userNameSmile);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userNameSmile);
    });

    it("should allow hieroglyphic symbols in First and Last name fields, and save it", function () {
        p.getByUrl();

        p.firstNameInput.clear();
        p.firstNameInput.sendKeys(p.userNameHierog);
        p.lastNameInput.clear();
        p.lastNameInput.sendKeys(p.userNameHierog);

        p.saveButton.click();

        p.catchSuccessAlert(p.saveSuccessAlert, 'Your account was successfully saved.');

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.userNameHierog);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.userNameHierog);
    });

    it("should open change password page", function () {
        p.getHomePage();

        expect(p.userAccountDropdownToggle.isDisplayed()).toBe(true);

        p.userAccountDropdownToggle.click();
        p.changePasswordLink.click();

        expect(browser.getCurrentUrl()).toContain('#/account/password');
        expect(p.htmlBody.getText()).toContain('Current password');
    });

    it("should allow to change password (and change it back)", function () {
        p.getPasswordByUrl();

        p.currentPasswordInput.sendKeys(p.userPassword);
        p.passwordInput.sendKeys(p.userPassword);
        p.submitButton.click();

        p.catchSuccessAlert(p.passwordCahngeSuccessAlert, 'Your password was successfully changed.');
    });

    xit("should not allow to change password if old password is wrong", function () {
        expect('test').toBe('written');
    });

    xit("should not allow to change password if new password is the same as old password", function () {
        expect('test').toBe('written');
    });

    p.passwordField.check(p, p.passwordUrl);

    xit("should save new password correctly", function () {
        expect('test').toBe('written');
    });
    xit("should do sth", function () {
        expect('test').toBe('written');
    });
    xit("should do sth", function () {
        expect('test').toBe('written');
    });
});