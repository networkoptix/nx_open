'use strict';
var Page = require('./po.js');

var protractor = require('protractor');
describe('Help Page', function () {

    var p = new Page();
    var ptor = protractor.getInstance();

    it("Should show link for api documentation",function(){
        p.get();
        expect(p.apiLink.isDisplayed()).toBe(true);
        expect(p.apiLink.getText()).toMatch("API");
        expect(p.apiLink.getAttribute("href")).toEqual("api.xmp");
    });

    it("Should show sdk",function(){
        expect("all tests").toBe("written");

        expect(p.sdkLink.isDisplayed()).toBe(true);
        expect(p.sdkLink.getText()).toMatch("SDK");
        p.sdkLink.click();
        expect(ptor.currentUrl()).toContain('#/sdkeula');

    });

    it("Should show require sdk eula to be accepted",function(){
        expect(this.sdkDownloadButton.isEnabled()).toBe(false);
        expect(this.acceptEulaCheckbox.isSelected()).toBe(false);
        this.acceptEulaCheckbox.click();
        expect(this.acceptEulaCheckbox.isSelected()).toBe(true);
        expect(this.sdkDownloadButton.isEnabled()).toBe(true);
    });

});