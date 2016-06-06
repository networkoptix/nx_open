'use strict';
var Page = require('./po.js');

var protractor = require('protractor');
describe('Developers Page', function () {

    var p = new Page();

    beforeEach(function() {
        p.get();
    });

    it("Link for server api documentation is displayed",function() {
        expect(p.apiLink.isDisplayed()).toBe(true);
        expect(p.apiLink.getText()).toMatch("API");
        expect(p.apiLink.getAttribute("href")).toMatch("api.xml");
        p.apiLink.click();
        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindowHandle = handles[0];
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle);
            browser.ignoreSynchronization = true;
            expect(browser.getCurrentUrl()).toContain('/api.xml'); // Check that url is correct
            browser.ignoreSynchronization = false;

            //Switch back
            browser.close();
            browser.switchTo().window(oldWindowHandle);
        });
    });

    // Disabled because /api.xml can not be opened in local build
    xit("Link for server api documentation opens /static/api.xml",function() {
        p.apiLink.click();
        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindowHandle = handles[0];
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle);
            expect(p.body.getText()).toContain('This group contains functions ' +
                'related to whole system (all servers).');

            //Switch back
            browser.close();
            browser.switchTo().window(oldWindowHandle);
        });
    });

    it("Sdk video link opens /static/index.html#/sdkeula/sdk, where EULA can be accepted",function(){
        expect(p.sdkLinkVideo.isDisplayed()).toBe(true);
        expect(p.sdkLinkVideo.getText()).toMatch("SDK");
        expect(p.sdkLinkVideo.getAttribute("href")).toMatch("sdkeula/sdk");
        p.sdkLinkVideo.click();

        expect(browser.getCurrentUrl()).toContain('sdkeula/sdk'); // Check that url is correct
        expect(p.body.getText()).toContain('Software Developer Kit End User Licence Agreement');
        expect(p.sdkDownloadButton.isEnabled()).toBe(false);
        expect(p.acceptEulaCheckbox.isSelected()).toBe(false);
        p.acceptEulaCheckbox.click();
        expect(p.acceptEulaCheckbox.isSelected()).toBe(true);
        expect(p.sdkDownloadButton.isEnabled()).toBe(true);
    });

    it("Sdk storage link opens /static/index.html#/sdkeula/storage_sdk, where EULA can be accepted",function(){
        expect(p.sdkLinkStorage.isDisplayed()).toBe(true);
        expect(p.sdkLinkStorage.getText()).toMatch("SDK");
        expect(p.sdkLinkStorage.getAttribute("href")).toMatch("sdkeula/storage_sdk");
        p.sdkLinkStorage.click();

        expect(browser.getCurrentUrl()).toContain('sdkeula/storage_sdk'); // Check that url is correct
        expect(p.body.getText()).toContain('Software Developer Kit End User Licence Agreement');
        expect(p.sdkDownloadButton.isEnabled()).toBe(false);
        expect(p.acceptEulaCheckbox.isSelected()).toBe(false);
        p.acceptEulaCheckbox.click();
        expect(p.acceptEulaCheckbox.isSelected()).toBe(true);
        expect(p.sdkDownloadButton.isEnabled()).toBe(true);
    });
});
