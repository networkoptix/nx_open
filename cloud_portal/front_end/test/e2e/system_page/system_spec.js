'use strict';
var SystemPage = require('./po.js');
describe('System suite', function () {

    var p = new SystemPage();

    afterEach(function() {
        p.helper.logout()
    });

    it("has system name, owner and OpenInNx button visible on every system page", function() {
        p.helper.loginToSystems(p.helper.userEmailAdmin, p.helper.userPassword);
        browser.sleep(2000);

        p.systemsList.map(function (elem, index) {
            return {
                index: index,
                sysName: elem.element(by.css('h2')).getText(),
                sysOwner: elem.element(by.css('.user-name')).getText()
            }
        }).then(function(systemsAttr) {
            p.systemsList.count().then(function(count) {
                for (var i= 0; i < count; i++) {
                    p.systemsList.get(i).click();
                    expect(browser.getCurrentUrl()).toContain('/systems/');
                    expect(p.systemNameElem.getText()).toContain(systemsAttr[i].sysName);
                    expect(p.systemOwnElem.getText()).toContain(systemsAttr[i].sysOwner);
                    expect(p.openInClientButton.isDisplayed()).toBe(true);

                    p.helper.navigateBack();
                }
            });
        });
    });
    xit("has system status visible if system is not online", function() {
    });
    it("has OpenInNx button disabled if system is not online", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner, p.helper.userPassword);
        var offlineSystems = p.systemsList.filter(function(elem) {
            // First filter systems that are not activated or offline
            return elem.getText().then(function(content) {
                return (p.helper.isSubstr(content, 'not activated') || p.helper.isSubstr(content, 'offline'))
            });
        });

        offlineSystems.count().then(function(count) {
            for (var i= 0; i < count; i++) {
                offlineSystems.get(i).click();

                expect(p.openInClientButton.isEnabled()).toBe(false);

                p.helper.navigateBack();
            }
        });
    }, 240000);

    it("should confirm, if owner deletes system (You are going to disconnect your system from cloud)", function() {
        p.helper.loginToSystems(p.helper.userEmailOwner, p.helper.userPassword);
        p.ownedSystem.click();
        p.ownerDeleteButton.click();
        expect(p.disconnectDialog.isDisplayed()).toBe(true);
        expect(p.disconnectDialog.getText()).toContain('Disconnect System from Nx Cloud?\n'+
            'All cloud users will be deleted.\n'+
            'System will be accessible through local network with local administrator account.');
        p.cancelDisconnectButton.click();
    });

    it("should confirm, if not owner deletes system (You will loose access to this system)", function() {
        p.helper.loginToSystems(p.helper.userEmailViewer, p.helper.userPassword);
        p.ownedSystem.click();
        p.userDeleteButton.click();
        expect(p.disconnectDialog.isDisplayed()).toBe(true);
        expect(p.disconnectDialog.getText()).toContain('Disconnect System?\nYou are about to disconnect this System '+
            'from your account. You will not be able to access this System anymore. Are you sure?');
        p.cancelDisconnectButton.click();
    });

    it("has Share button, visible for admin and owner", function() {
        p.helper.loginToSystems(p.helper.userEmailAdmin, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isDisplayed()).toBe(true);

        p.helper.logout();
        p.helper.login(p.helper.userEmailOwner, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isDisplayed()).toBe(true);
    });

    it("does not show Share button to viewer, advanced viewer, live viewer", function() {
        p.helper.loginToSystems(p.helper.userEmailViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isPresent()).toBe(false);

        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailAdvViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isPresent()).toBe(false);

        p.helper.logout();
        p.helper.loginToSystems(p.helper.userEmailLiveViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isPresent()).toBe(false);
    });

    it("should open System page by link to not authorized user and redirect to homepage, if he does not log in", function() {
        browser.get(p.url + p.systemLink);
        // Check that system page is not displayed and login form pops up

        // Error alert used to be here. Now it's not
        // p.alert.catchAlert(p.alert.alertMessages.systemAccessError, p.alert.alertTypes.danger);

        expect(p.systemNameElem.getText()).not.toContain(p.systemName);
        expect(p.loginButton.isPresent()).toBe(true);

        p.loginCloseButton.click();
        // Check that system page is not displayed and user is on homepage
        expect(p.systemNameElem.getText()).not.toContain(p.systemName);
        expect(browser.getCurrentUrl()).not.toContain(p.url);
    });

    it("should open System page by link to not authorized user and show it, after owner logs in", function() {
        browser.get(p.url + p.systemLink);
        // Check that system page is not displayed and login form pops up

        //Error alert used to be here. Now it's not

        expect(p.systemNameElem.getText()).not.toContain(p.systemName);
        expect(p.loginButton.isPresent()).toBe(true);
        // Fill data into login page
        p.helper.loginFromCurrPage(p.helper.userEmailOwner, p.helper.userPassword);
        expect(p.systemNameElem.getText()).toContain(p.systemName);
    });

    it("should open System page by link to user without permission and show alert (System info is unavailable: You have no access to this system)", function() {
        p.helper.loginToSystems(p.helper.userEmailNoPerm, p.helper.userPassword);

        //p.helper.getSysPage(p.systemLink);
        browser.get(p.url + p.systemLink);
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.systemAccessRestricted)
    });

    it("should open System page by link not authorized user, and show alert if logs in and has no permission", function() {
        browser.get(p.url + p.systemLink);
        // Check that system page is not displayed and login form pops up

        //Error alert used to be here. Now it's not
        //p.alert.catchAlert(p.alert.alertMessages.systemAccessError, p.alert.alertTypes.danger);

        expect(p.systemNameElem.getText()).not.toContain(p.systemName);
        expect(p.loginButton.isPresent()).toBe(true);
        // Fill data into login page
        p.helper.loginFromCurrPage(p.helper.userEmailNoPerm, p.helper.userPassword);
        expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.systemAccessRestricted)
    });

    it("should display same user data as user provided during registration (stress to cyrillic)", function() {
        p.helper.createUser(p.helper.userNameCyrillic, p.helper.userNameCyrillic).then( function(userEmail) {
            expect(p.helper.htmlBody.getText()).toContain(p.alert.alertMessages.registerConfirmSuccess);

            p.helper.login(p.helper.userEmailOwner, p.helper.userPassword);
            p.helper.getSysPage(p.systemLink);
            p.shareButton.click();
            p.emailField.sendKeys(userEmail);
            p.roleField.click();
            p.roleOptionAdmin.click();
            p.submitShareButton.click();

            p.helper.logout();
            p.helper.loginToSystems(userEmail, p.helper.userPassword);
            p.helper.getSysPage(p.systemLink);
            expect(p.usrDataRow(userEmail).getText()).toContain(p.helper.userNameCyrillic + ' ' + p.helper.userNameCyrillic);
        });
    }, 180000);

    it("should display same user data as showed in user account (stress to cyrillic)", function() {
        p.helper.loginToSystems(p.helper.userEmailAdmin);
        p.helper.get(p.helper.urls.account);

        p.helper.changeAccountNames(p.helper.userNameCyrillic, p.helper.userNameCyrillic);
        p.helper.getSysPage(p.systemLink);
        expect(p.usrDataRow(p.helper.userEmailAdmin).getText()).toContain(p.helper.userNameCyrillic + ' ' + p.helper.userNameCyrillic);
    });
});
