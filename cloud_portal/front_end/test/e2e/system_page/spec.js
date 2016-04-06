'use strict';
var SystemPage = require('./po.js');
describe('System suite', function () {

    var p = new SystemPage();

    beforeAll(function() {
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        console.log("\nSystem page start\n");
    });

    afterAll(function() {
        p.helper.logout();
        console.log("\nSystem page finish\n");
    });

    it("has system name, owner and OpenInNx button visible on every system page", function() {
        p.systemsList.map(function (elem, index) {
            return {
                index: index,
                sysName: elem.element(by.css('h2')).getText(),
                sysOwner: elem.element(by.css('.system-owner')).getText()
            }
        }).then(function(systemsAttr) {
            p.systemsList.count().then(function(count) {
                for (var i= 0; i < count; i++) {
                    p.systemsList.get(i).click();
                    expect(browser.getCurrentUrl()).toContain('#/systems/');
                    expect(p.systemName.getText()).toContain(systemsAttr[i].sysName);
                    expect(p.systemOwn.getText()).toContain(systemsAttr[i].sysOwner);
                    expect(p.openInNxButton.isDisplayed()).toBe(true);

                    p.helper.navigateBack();
                }
            });
        });
    });
    xit("has system status visible if system is not online", function() {
    });
    it("has OpenInNx button disabled if system is not online", function() {
        var offlineSystems = p.systemsList.filter(function(elem) {
            // First filter systems that are not activated or offline
            return elem.getInnerHtml().then(function(content) {
                return (p.helper.isSubstr(content, 'not activated') || p.helper.isSubstr(content, 'offline'))
            });
        });
        offlineSystems.count().then(function(count) {
            for (var i= 0; i < count; i++) {
                offlineSystems.get(i).click();
                expect(p.openInNxButton.isEnabled()).toBe(false);

                p.helper.navigateBack();
            }
        });
    });
    xit("should confirm, if owner deletes system ( You are going to disconnect your system from cloud)", function() {

    });
    xit("should confirm, if not owner deletes system (You will loose access to this system)", function() {

    });

    it("has Share button, visible for admin and owner", function() {
        p.helper.logout();
        p.helper.login(p.userEmailAdmin, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isDisplayed()).toBe(true);

        p.helper.logout();
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isDisplayed()).toBe(true);
    });

    it("does not show Share button to viewer, advanced viewer, live viewer", function() {
        p.helper.logout();
        p.helper.login(p.userEmailViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isPresent()).toBe(false);

        p.helper.logout();
        p.helper.login(p.userEmailAdvViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isPresent()).toBe(false);

        p.helper.logout();
        p.helper.login(p.userEmailLiveViewer, p.helper.userPassword);
        p.ownedSystem.click();
        expect(p.shareButton.isPresent()).toBe(false);

        p.helper.logout();
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
    });

    xit("should open System page by link to not authorized user and show it, after he logs in", function() {

    });
    xit("should open System page by link to not authorized user and redirect to homepage, if he does not log in", function() {

    });
    xit("should open System page by link to user without permission and show alert (System info is unavailable: You have no access to this system)", function() {

    });
    xit("should open System page by link not authorized user, and not only show alert if logs in and has no permission", function() {

    });
    xit("should display same user data as user provided during registration (stress to cyrillic, hieroglyph, etc)", function() {

    });
    xit("should display same user data as showed in user account (stress to cyrillic, hieroglyph, etc)", function() {

    });
});