'use strict';
var Page = require('./po.js');

var protractor = require('protractor');
describe('Developers Page', function () {
    //it("should stop test",function(){expect("other test").toBe("uncommented");});return;

    var p = new Page();

    it("Should show link for api documentation",function(){
        p.get();
        expect(p.apiLink.isDisplayed()).toBe(true);
        expect(p.apiLink.getText()).toMatch("API");
        expect(p.apiLink.getAttribute("href")).toMatch("api.xml");
    });

    it("Should show sdk video link",function(){
        expect(p.sdkLinkVideo.isDisplayed()).toBe(true);
        expect(p.sdkLinkVideo.getText()).toMatch("SDK");
    });

    it("Should show sdk storage link",function(){
        expect(p.sdkLinkStorage.isDisplayed()).toBe(true);
        expect(p.sdkLinkStorage.getText()).toMatch("SDK");
    });

    it("Should show require sdk eula to be accepted",function(){

        p.sdkLinkVideo.click();
        expect(p.sdkDownloadButton.isEnabled()).toBe(false);
        expect(p.acceptEulaCheckbox.isSelected()).toBe(false);
        p.acceptEulaCheckbox.click();
        expect(p.acceptEulaCheckbox.isSelected()).toBe(true);
        expect(p.sdkDownloadButton.isEnabled()).toBe(true);
    });

});
