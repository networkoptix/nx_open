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
        p.helper.get(p.helper.urls.account);
    });

    p.alert.checkAlert(function(){
        var deferred = protractor.promise.defer();
        p.alert.submitButton.click();
        deferred.fulfill();
        return deferred.promise;
    }, p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success, true);

    it("dropdown in top right corner has links: Account settings, Change password, Logout", function () {
        p.helper.get(p.helper.urls.homepage);
        var dropdownMenu = p.helper.forms.logout.dropdownMenu;
        var dropdownToggle = p.helper.forms.logout.dropdownToggle;

        dropdownToggle.click();
        expect(p.helper.forms.logout.dropdownParent.getText()).toContain(p.helper.userEmail);
        expect(dropdownMenu.getText()).toContain('Account Settings');
        expect(dropdownMenu.getText()).toContain('Change Password');
        expect(dropdownMenu.getText()).toContain('Log Out');
        dropdownToggle.click();
    });

    it("it is possible to log out", function () {
        p.helper.get(p.helper.urls.homepage);
        //element(by.css('header')).element(by.css('.navbar')).element(by.css('a[uib-dropdown-toggle]')).click();
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

        //p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success, 0);

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

        //p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameCyrillic);
    });

    it("smile symbols in First and Last name fields are allowed and correctly saved", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameSmile);
        p.lastNameInput.clear().sendKeys(p.helper.userNameSmile);

        p.saveButton.click();

        //p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameSmile);
    });

    it("hieroglyphic symbols in First and Last name fields are allowed and correctly saved", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userNameHierog);
        p.lastNameInput.clear().sendKeys(p.helper.userNameHierog);

        p.saveButton.click();

        //p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();

        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.userNameHierog);
    });

    it("more than 256 symbols can be entered in each field and then are cut to 256", function () {
        p.firstNameInput.clear().sendKeys(p.helper.inputLong300);
        p.lastNameInput.clear().sendKeys(p.helper.inputLong300);
        p.saveButton.click();
        //p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);

        p.refresh();
        expect(p.firstNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
        expect(p.lastNameInput.getAttribute('value')).toMatch(p.helper.inputLongCut);
    });

    it("pressing Enter key saves data", function () {
        p.firstNameInput.clear().sendKeys(p.helper.userFirstName);
        p.lastNameInput.clear().sendKeys(p.helper.userLastName).sendKeys(protractor.Key.ENTER);

        //p.alert.catchAlert( p.alert.alertMessages.accountSuccess, p.alert.alertTypes.success);
    });

    it("pressing Tab key moves focus to the next element", function () {
        // Navigate to next field using TAB key
        p.firstNameInput.sendKeys(protractor.Key.TAB);
        p.helper.checkElementFocusedBy(p.lastNameInput, 'id');
    });

    it("should apply changes if fields are edited in two browser instances", function () {
        var browser2 = browser.forkNewDriverInstance();
        var firstNameInput2 = browser2.element(by.model('account.first_name'));
        var saveButton2 = browser2.element(by.css('[form=accountForm]')).element(by.buttonText('Save'));
        var loginEmailInput2 = browser2.element(by.css('.modal-dialog')).element(by.model('auth.email'));
        var loginPasswordInput2 = browser2.element(by.css('.modal-dialog')).element(by.model('auth.password'));
        var loginSubmitButton2 = browser2.element(by.css('.modal-dialog')).element(by.buttonText('Log in'));


        // Log in in browser2
        browser2.get(p.helper.urls.homepage);
        browser2.waitForAngular();
        browser2.sleep(500);
        browser2.element(by.css('a[href="/login"]')).click();
        loginEmailInput2.sendKeys(p.helper.userEmail);
        loginPasswordInput2.sendKeys(p.helper.userPassword);
        loginSubmitButton2.click();
        browser.sleep(2000);

        // Make changes in browser2
        browser2.get(p.helper.urls.account);
        firstNameInput2.clear().sendKeys('NameInBrowser2');
        saveButton2.click();
        // check that changes in first browser instance correspond to second browser
        browser.refresh();
        expect(p.firstNameInput.getAttribute('value')).toContain('NameInBrowser2');

        p.firstNameInput.clear().sendKeys('NameInBrowser1');
        p.saveButton.click();
        // check that changes in second browser instance correspond to first browser
        browser2.refresh();
        expect(firstNameInput2.getAttribute('value')).toContain('NameInBrowser1');
    });
});