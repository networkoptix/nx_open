'use strict';
var Page = require('./po.js');
describe('Information Page', function () {

    var p = new Page();

    beforeEach(function(){
        p.get();
    });

    it("should show software info",function(){
        expect(p.versionNode.getText()).toMatch(/\d+\.\d+\.\d+.\d+/);//2.3.0.1234
        expect(p.archNode.getText()).toMatch(/[\w\d]+/);//x86,arm,x64
        expect(p.platformNode.getText()).toMatch(/\w+/); //linux,mac,windows
    });

    it("should show storage info",function(){
        expect(p.storagesNodes.count()).toBeGreaterThan(0); // storages exist
        expect(p.storagesNodes.first().element(by.css(".storage-url")).getText()).toMatch(/[\w\d/\\]+/); // Except one good url
        expect(p.storagesNodes.first().element(by.css(".storage-total-space")).getText()).toMatch(/[\d+\.?\d*\s*\wB]+/);// GB,  TB  - it's size
        expect(p.storagesNodes.first().element(by.css(".storage-indicators")).element(by.css("[title~=Internal]")).isDisplayed()).toBe(true); // At least one indicator
    });

    it("health monitoring: should display legend",function(){
        //var EC = protractor.ExpectedConditions;
        //var legend = $('.legend-checkbox');
        //var isPresent = EC.presenceOf(legend);
        browser.sleep(2000);

        browser.ignoreSynchronization = true;

        //browser.wait(isPresent, 5000); //wait for an element to become present
        expect(element(by.css('.legend-checkbox')).isPresent()).toBe(true);
        expect(p.hmLegendNodes.count()).toBeGreaterThan(1); //legend exists, except 2+ elements

        browser.ignoreSynchronization = false;
    });

     xit("health monitoring: should update graphics, should show message when server is offline, should enable-disable indicators using checkboxes",function(){
         expect("health monitoring").toBe("tested manually");
     });

    it("log: should show iframe",function(){
        expect(p.logIframe.isDisplayed()).toBe(true);
        expect(p.logIframe.getAttribute("src")).toMatch("/api/showLog");
    });

    it("log: should show refresh link to update iframe",function(){
        expect(p.refreshLogButton.isDisplayed()).toBe(true);
        expect(p.refreshLogButton.getText()).toEqual("Refresh");
        expect(p.refreshLogButton.getAttribute("target")).toEqual(p.logIframe.getAttribute("name"));
        expect(p.refreshLogButton.getAttribute("href")).toEqual(p.logIframe.getAttribute("src"));
    });

    it("log: should have a link to open log in new window",function(){
        expect(p.openLogButton.isDisplayed()).toBe(true);
        expect(p.openLogButton.getText()).toEqual("Open in new window");
        expect(p.openLogButton.getAttribute("target")).toEqual("_blank");
        expect(p.openLogButton.getAttribute("href")).toMatch("/api/showLog");
    });
});