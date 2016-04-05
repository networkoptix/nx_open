'use strict';
var SystemsListPage = require('./po.js');
describe('Systems list suite', function () {

    var p = new SystemsListPage();

    beforeAll(function() {
        p.helper.get('/');
        p.helper.login(p.helper.userEmail, p.helper.userPassword);
        console.log("Systems start");
    });

    afterAll(function() {
        p.helper.logout();
        console.log("Systems finish");
    });

    beforeEach(function() {
        //p.helper.get(p.url);
    });

    it("should show list of Systems", function () {
        expect(browser.getCurrentUrl()).toContain('systems');
        expect(p.htmlBody.getText()).toContain('Systems');

        expect(p.systemsList.first().isDisplayed()).toBe(true);
    });

    it("should show Open in NX client button for every online system", function () {
        p.systemsList.each(function (element, index) {
            browser.sleep(5000);
            element.getInnerHtml().then(function(content) {
                if(content.indexOf('not activated') <= -1) {
                    expect(element.element(by.buttonText('Open in Nx Witness')).isDisplayed()).toBe(true);
                }
                console.log(index);
            });
        });
    });
    //xit("should оpen in NX client, when clicking 'Open in NX client' button", function () {
    //    expect("test").toBe("written");
    //});
    //xit("should not open system page, when clicking 'Open in NX client'", function () {
    //    expect("test").toBe("written");
    //});
    //xit("should not show Open in NX client button for offline system", function () {
    //    expect("test").toBe("written");
    //});
    //xit("should show system's state for systems (activated, not activated, online, offline)", function () {
    //    expect("test").toBe("written");
    //});
    //xit("should show system's owner for every system in the list, 'Your system' for user's systems", function () {
    //    expect("test").toBe("written");
    //});
    //xit("should show email for every system, if system owner's name is empty", function () {
    //    expect("test").toBe("written");
    //});
    //xit("should opens system page (users list) when clicked on system", function () {
    //    expect("test").toBe("written");
    //});
    //xit("should update owner name in systems list, if it's changed", function () {
    //    expect("test").toBe("written");
    //});
});