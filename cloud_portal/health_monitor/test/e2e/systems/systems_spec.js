'use strict';
var SystemsListPage = require('./po.js');
describe('Systems list suite', function () {

    var p = new SystemsListPage();

    beforeAll(function() {
        p.helper.login(p.helper.userEmailOwner, p.helper.userPassword);
        p.helper.get('/systems');
    });

    beforeEach(function() {
        jasmine.addMatchers(p.customMatchers);
    });

    afterAll(function() {
        p.helper.logout();
    });

    it("should show list of Systems", function () {
        expect(browser.getCurrentUrl()).toContain('systems');
        expect(p.helper.htmlBody.getText()).toContain('Systems');
        expect(p.helper.allSystems.first().isDisplayed()).toBe(true);
    });

    it("should show Open in NX client button for every online system", function () {
        p.helper.allSystems.filter(function(elem) {
            // First filter systems that are activated
            return elem.getText().then(function(text) {
                return !p.helper.isSubstr(text, 'offline');
            });
        }).each(function (elem) {
            // Then for every activated system, check that button is visible
            expect(p.openInNxButton(elem).isDisplayed()).toBe(true);
        });
    });

    it("should not show Open in NX client button for offline system", function () {
        p.helper.allSystems.filter(function(elem) {
            // First filter systems that are not activated or offline
            return elem.getText().then(function(text) {
                return (p.helper.isSubstr(text, 'offline'))
            });
        }).each(function (elem) {
            // Then for every such system, check that button is not visible
            expect(p.openInNxButton(elem).isPresent()).toBe(false);
        });
    });

    it("should show system's state for systems if they are offline. Otherwise - button Open in Nx", function () {
        p.helper.allSystems.each(function (elem) {
            expect(elem.getText()).toContainAnyOf(['Open in ','offline']);
        });
    });

    it("should not show not activated systems in the list", function () {
        p.helper.allSystems.each(function (elem) {
            expect(elem.getText()).not.toContain('not activated');
        });
    });

    xit("should show system's owner for every system in the list, 'Your system' for user's systems", function () {
        p.helper.allSystems.each(function (elem) {
            expect(p.systemOwner(elem).getText()).toContain('Your system'); // how to find out, who is the owner?
        });
    });

    xit("should show email for every system, if system owner's name is empty", function () {
        // system owner name is required now. it can't be empty
        p.helper.allSystems.each(function (elem) {
            expect(p.systemOwner(elem).getText()).toContain('Your system');
        });
    });

    it("should open system page (users list) when clicked on system", function () {
        p.helper.allSystems.count().then(function(count) {
            for (var i= 0; (i < count) && (i < 10); i++) { // Do not check more than 10 systems
                p.helper.allSystems.get(i).click();
                // TODO: Write appropriate checks that system page is opened
                expect(browser.getCurrentUrl()).toContain('/systems/');
                p.helper.navigateBack();
            }
        });
    });

    it("should update owner name in systems list, if it's changed", function () {
        var newFirstName = 'newFirstName';
        var newLastName = 'newLastName';
        p.helper.openSystemFromSystemList(p.helper.systemName);
        p.helper.shareSystemWith(p.helper.userEmailViewer, p.helper.roles.viewer);
        p.helper.changeAccountNames(newFirstName, newLastName);
        p.helper.logout();
        p.helper.login(p.helper.userEmailViewer, p.helper.userPassword);

        expect(p.systemOwner(p.helper.onlineSystemsWithCurrentName.first()).getText()).toContain(newFirstName);
        expect(p.systemOwner(p.helper.onlineSystemsWithCurrentName.first()).getText()).toContain(newLastName);

        p.helper.logout();
        p.helper.login(p.helper.userEmailAdmin, p.helper.userPassword);
    });

    xit("should display systems sorted (status, then owner); user's systems go first", function () {
    });
});