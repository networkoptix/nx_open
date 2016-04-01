'use strict';
var AccountPage = require('./po.js');
describe('Account suite', function () {

    var p = new AccountPage();

    beforeAll(function() {
        console.log('\nAccount start\n');
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    afterAll(function() {
        console.log('\nAccount finish\n');
        p.helper.logout();
    });

    beforeEach(function() {
        p.get(p.accountUrl);
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
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
        expect(p.emailField.getAttribute('value')).toContain(p.helper.userEmail);
        expect(p.firstNameInput.isDisplayed()).toBe(true);
        expect(p.lastNameInput.isDisplayed()).toBe(true);
        expect(p.subscribeCheckbox.isDisplayed()).toBe(true);
    });

    it("should allow to edit first name, last name, subscribe, and save it", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear().sendKeys(p.helper.userLastName);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userFirstName);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userLastName);
    });

    it("should not allow to save empty First and Last name", function () {
        p.firstNameInput.clear();
        p.lastNameInput.clear();

        p.saveButton.click();

        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
        p.checkInputInvalid(p.lastNameInput, p.invalidClassRequired);
    });

    it("should not allow to save empty First name", function () {
        p.firstNameInput.clear();
        p.lastNameInput.clear().sendKeys(p.helper.userLastName);

        p.saveButton.click();
        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
    });

    it("should not allow to save empty Last name", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear();

        p.saveButton.click();
        p.checkInputInvalid(p.lastNameInput, p.invalidClassRequired);
    });

    it("should allow cyrillic First and Last names, and save it", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameCyrillic);
        p.lastNameInput.clear().sendKeys(p.helper.userNameCyrillic);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
    });

    it("should allow smile symbols in First and Last name fields, and save it", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameSmile);
        p.lastNameInput.clear().sendKeys(p.helper.userNameSmile);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
    });

    it("should allow hieroglyphic symbols in First and Last name fields, and save it", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameHierog);
        p.lastNameInput.clear().sendKeys(p.helper.userNameHierog);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
    });

    it("should allow to enter more than 256 symbols in each field and cut it to 256", function () {
        p.firstNameInput.clear().sendKeys(p.helper.inputLong300);
        p.lastNameInput.clear().sendKeys(p.helper.inputLong300);
        p.saveButton.click();
        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();
        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
    });

    it("should respond to Enter key and save data", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear().sendKeys(p.helper.userLastName).sendKeys(protractor.Key.ENTER);

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);
    });

    it("should respond to Tab key", function () {
        // Navigate to next field using TAB key
        p.firstNameInput.sendKeys(protractor.Key.TAB);
        p.helper.checkElementFocusedBy(p.lastNameInput, 'id');
    });

    xit("should apply changes if fields are edited in two browser instances", function () {
    });
});