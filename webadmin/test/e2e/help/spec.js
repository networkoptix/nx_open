'use strict';
var Page = require('./po.js');
describe('Help Page', function () {
    var p = new Page();

    beforeEach( function() {
        p.get();
    });

    it("should show some links on the left",function(){
        expect (p.links.count()).toBeGreaterThan(0);
        var firstLink = p.links.first().element(by.tagName("a"));
        expect(firstLink.isPresent()).toBe(true);
    });

    it("Get Support button opens external support page",function(){
        expect(p.supportLink.isPresent()).toBe(true);
        p.supportLink.click();
        p.helper.performAtSecondTab( function() {
            p.helper.ignoreSyncFor( function() {
                expect(browser.getCurrentUrl()).toContain('/hc/');
                expect(p.body.getText()).toContain('KNOWN ISSUES');
            });
            browser.close();
        });
    });

    it("Calculate button opens external hardware calculator",function(){
        expect(p.calculatorLink.isPresent()).toBe(true);
        p.calculatorLink.click();
        p.helper.performAtSecondTab( function() {
            p.helper.ignoreSyncFor( function() {
                expect(browser.getCurrentUrl()).toContain('networkoptix.com/calculator');
            });
            browser.close();
        });
    });

    it("Android moblie application link opens google play app page",function(){
        expect(p.androidStoreButton.isPresent()).toBe(true);
        p.androidStoreButton.click();
        p.helper.performAtSecondTab( function() {
            p.helper.ignoreSyncFor( function() {
                expect(browser.getCurrentUrl()).toContain('play.google.com/store/apps/details?id=com.networkoptix.hdwitness');
            });
            browser.close();
        });
    });

    it("Ios moblie application link opens itunes app page",function(){
        expect(p.appleStoreButton.isPresent()).toBe(true);
        p.appleStoreButton.click();
        p.helper.performAtSecondTab( function() {
            p.helper.ignoreSyncFor( function() {
                expect(browser.getCurrentUrl()).toContain('itunes.apple.com/eg/app/hd-witness/');
            });
            browser.close();
        });
    });
});