'use strict';
var AccountPage = require('./po.js');
describe('On account page,', function () {

    var p = new AccountPage();

    beforeAll(function() {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    afterAll(function() {
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

    it("dropdown in top right corner has links: Account settings, Change password, Logout", function () {
        p.get(p.homePageUrl);
        expect(p.userAccountDropdownToggle.getText()).toContain(p.helper.userEmail);

        p.userAccountDropdownToggle.click();
        expect(p.userAccountDropdownMenu.getText()).toContain('Account Settings');
        expect(p.userAccountDropdownMenu.getText()).toContain('Change Password');
        expect(p.userAccountDropdownMenu.getText()).toContain('Logout');
    });

    it("it is possible to log out", function () {
        p.get(p.homePageUrl);
        p.helper.logout();
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    it("there are fields: email, first name, last name, subscribe checkbox", function () {
        expect(p.emailField.getAttribute('value')).toContain(p.helper.userEmail);
        expect(p.firstNameInput.isDisplayed()).toBe(true);
        expect(p.lastNameInput.isDisplayed()).toBe(true);
        expect(p.subscribeCheckbox.isDisplayed()).toBe(true);
    });

    it("first name, last name, subscribe are editable and can be saved", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear().sendKeys(p.helper.userLastName);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userFirstName);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userLastName);
    });

    it("First name and Last name are required", function () {
        p.firstNameInput.clear();
        p.lastNameInput.clear();

        p.saveButton.click();

        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
        p.checkInputInvalid(p.lastNameInput, p.invalidClassRequired);
    });

    it("First name is required", function () {
        p.firstNameInput.clear();
        p.lastNameInput.clear().sendKeys(p.helper.userLastName);

        p.saveButton.click();
        p.checkInputInvalid(p.firstNameInput, p.invalidClassRequired);
    });

    it("Last name is required", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear();

        p.saveButton.click();
        p.checkInputInvalid(p.lastNameInput, p.invalidClassRequired);
    });

    it("cyrillic First and Last names are allowed and correctly saved", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameCyrillic);
        p.lastNameInput.clear().sendKeys(p.helper.userNameCyrillic);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
    });

    it("smile symbols in First and Last name fields are allowed and correctly saved", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameSmile);
        p.lastNameInput.clear().sendKeys(p.helper.userNameSmile);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
    });

    it("hieroglyphic symbols in First and Last name fields are allowed and correctly saved", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameHierog);
        p.lastNameInput.clear().sendKeys(p.helper.userNameHierog);

        p.saveButton.click();

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
    });

    it("more than 256 symbols can be entered in each field and then are cut to 256", function () {
        p.firstNameInput.clear().sendKeys(p.helper.inputLong300);
        p.lastNameInput.clear().sendKeys(p.helper.inputLong300);
        p.saveButton.click();
        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();
        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
    });

    it("pressing Enter key saves data", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear().sendKeys(p.helper.userLastName).sendKeys(protractor.Key.ENTER);

        p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);
    });

    it("pressing Tab key moves focus to the next element", function () {
        // Navigate to next field using TAB key
        p.firstNameInput.sendKeys(protractor.Key.TAB);
        p.helper.checkElementFocusedBy(p.lastNameInput, 'id');
    });

    xit("should apply changes if fields are edited in two browser instances", function () {
    });
});