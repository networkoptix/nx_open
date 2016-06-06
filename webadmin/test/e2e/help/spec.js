'use strict';
var Page = require('./po.js');
describe('Help Page', function () {
    var p = new Page();

    it("should show some links on the left",function(){
        p.get();
        expect (p.links.count()).toBeGreaterThan(0);
        var firstLink = p.links.first().element(by.tagName("a"));
        expect(firstLink.isPresent()).toBe(true);
    });

    it("Get Support button opens external support page",function(){
        expect(p.supportButton.isPresent()).toBe(true);
        p.supportButton.click();
        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindowHandle = handles[0];
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle);

            browser.ignoreSynchronization = true;
            expect(browser.getCurrentUrl()).toContain('/hc/');
            expect(p.body.getText()).toContain('KNOWN ISSUES');
            browser.ignoreSynchronization = false;

            //Switch back
            browser.close();
            browser.switchTo().window(oldWindowHandle);
        });
    });

    it("Calculate button opens external hardware calculator",function(){
        expect(p.calculatorButton.isPresent()).toBe(true);
        p.calculatorButton.click();
        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindowHandle = handles[0];
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle);

            browser.ignoreSynchronization = true;
            expect(browser.getCurrentUrl()).toContain('networkoptix.com/calculator');
            browser.ignoreSynchronization = false;

            //Switch back
            browser.close();
            browser.switchTo().window(oldWindowHandle);
        });
    });

    it("Android moblie application link opens google play app page",function(){
        expect(p.androidStoreButton.isPresent()).toBe(true);
        p.androidStoreButton.click();
        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindowHandle = handles[0];
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle);

            browser.ignoreSynchronization = true;
            expect(browser.getCurrentUrl()).toContain('play.google.com/store/apps/details?id=com.networkoptix.hdwitness');
            browser.ignoreSynchronization = false;

            //Switch back
            browser.close();
            browser.switchTo().window(oldWindowHandle);
        });
    });

    it("Ios moblie application link opens itunes app page",function(){
        expect(p.appleStoreButton.isPresent()).toBe(true);
        p.appleStoreButton.click();
        // Switch to just opened new tab
        browser.getAllWindowHandles().then(function (handles) {
            var oldWindowHandle = handles[0];
            var newWindowHandle = handles[1];
            browser.switchTo().window(newWindowHandle);

            browser.ignoreSynchronization = true;
            expect(browser.getCurrentUrl()).toContain('itunes.apple.com/eg/app/hd-witness/');
            browser.ignoreSynchronization = false;

            //Switch back
            browser.close();
            browser.switchTo().window(oldWindowHandle);
        });
    });
});