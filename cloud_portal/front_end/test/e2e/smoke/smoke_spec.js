'use strict';
var Helper = require('../helper.js');
this.helper = new Helper();
var helper = this.helper;
describe('Smoke test:', function () {

    var newUserEmail4RestorePass = helper.getRandomEmail();

    beforeAll(function() {
        helper.get();
    });

    afterEach(function () {
        helper.logout();
    });

    it("check needed user exists of create one", function () {
        helper.createUserIfMissing(null, null, helper.userEmailOwner);
        helper.createUserIfMissing(null, null, helper.userEmailSmoke);
        helper.createUserIfMissing(null, null, helper.userEmailCustom);
    });

    it("can login and logout", function () {
        helper.login();
    });

    it("can view system list", function () {
        helper.login();
        helper.get(helper.urls.systems);
        expect(browser.getCurrentUrl()).toContain('systems');
        expect(helper.htmlBody.getText()).toContain('Systems');
        expect(helper.allSystems.first().isDisplayed()).toBe(true);
        expect(element(by.cssContainingText('h2', helper.systemName)).isDisplayed()).toBe(true);
    });

    it("can view system page", function () {
        var userStrings = element.all(by.repeater('user in system.users'));
        var userList = helper.getParentOf(userStrings.first());

        helper.login();
        expect(browser.getCurrentUrl()).toContain('systems');

        // If system is the only one and it is already opened, do nothing.
        // Otherwise, open first online system with specified name
        element(by.cssContainingText('h1', helper.systemName)).isPresent().then(function (isPresent) {
            if(!isPresent) {
                expect(helper.getOnlineSystemsWithName(helper.systemName).first().isDisplayed()).toBe(true);
                helper.onlineSystemsWithCurrentName.first().click();
            }
        });

        browser.sleep(1000);
        helper.waitIfNotPresent(userList, 100);
        expect(userList.isDisplayed()).toBe(true);
    });

    it("can create new user", function () {
        helper.createUser(null, null, newUserEmail4RestorePass);
        helper.login(newUserEmail4RestorePass);
        expect(helper.loginSuccessElement.isPresent()).toBe(true);
    });

    it("can open system", function () {
        helper.login(helper.userEmailOwner);
        helper.getSysPage(helper.systemLink);
        expect(helper.loginSysPageSuccessElement.isPresent()).toBe(true);
    });

    it("can share system with existing user", function () {
        var newUserEmail = helper.getRandomEmail();
        helper.createUser('Mark', 'Hamill', newUserEmail);
        helper.login(helper.userEmailOwner);
        helper.openSystemFromSystemList(); // name will be taken from helper.systemName
        helper.shareSystemWith(newUserEmail, helper.roles.admin);
        helper.logout();
        // Now login with new user to see the system
        helper.login(newUserEmail);
        helper.openSystemByLink(helper.systemLink);
        expect(helper.loginSysPageSuccessElement.isPresent()).toBe(true);
    });

    it("can share system with non-existing user", function () {
        var newUserEmail = helper.getRandomEmail();

        helper.login(helper.userEmailOwner);
        helper.registerByInvite(helper.userFirstName, helper.userLastName, newUserEmail, helper.roles.admin);
        helper.sleepInIgnoredSync(2000);
        expect(helper.loginSysPageSuccessElement.isPresent()).toBe(true);
    });

    it("can restore password", function () {
        helper.restorePassword(helper.userEmailSmoke, 'qweasd1234');
        helper.login(helper.userEmailSmoke, 'qweasd1234');
        // expect(helper.loginNoSysSuccessElement.isPresent()).toBe(true);
    });

    it("can change user account data", function () {
        var here = helper.forms.account;

        helper.login();
        helper.get(helper.urls.account);

        here.firstNameInput.clear().sendKeys(helper.userFirstName);
        here.lastNameInput.clear().sendKeys(helper.userLastName);

        here.submitButton.click();
        helper.refresh();

        expect(here.firstNameInput.getAttribute('value')).toMatch(helper.userFirstName);
        expect(here.lastNameInput.getAttribute('value')).toMatch(helper.userLastName);
    });

    it("can change user password", function () {
        var here = helper.forms.password;

        helper.login();
        helper.get(helper.urls.password_change);

        here.currentPasswordInput.sendKeys(helper.userPassword);
        here.passwordInput.sendKeys(helper.userPasswordNew);
        here.submitButton.click();

        helper.logout();
        helper.login(helper.userEmailOwner, helper.userPasswordNew);

        helper.get(helper.urls.password_change);
        here.currentPasswordInput.sendKeys(helper.userPasswordNew);
        here.passwordInput.sendKeys(helper.userPassword);
        here.submitButton.click();
    });

    it("can view Downloads page", function () {
        helper.get();
        element(by.partialLinkText('Download')).click();
        expect(element(by.css('ul.nav-tabs')).getText()).toContain('Windows');
        expect(element(by.css('ul.nav-tabs')).getText()).toContain('Ubuntu Linux');
        expect(element(by.css('ul.nav-tabs')).getText()).toContain('Mac OS');

        element(by.cssContainingText('uib-tab-heading', 'Windows')).click();
        expect(element(by.css('.panel-default')).getText()).toContain('Download');

        element(by.cssContainingText('a.download-button', 'Download')).click();

        element(by.cssContainingText('uib-tab-heading', 'Linux')).click();
        expect(element(by.css('.tab-pane.active')).getText()).toContain('Download');

        element(by.cssContainingText('uib-tab-heading', 'Mac OS')).click();
        expect(element(by.css('.tab-pane.active')).getText()).toContain('Download');
    });
});