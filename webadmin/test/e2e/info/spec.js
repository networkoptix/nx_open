'use strict';
var Page = require('./po.js');
describe('Information Page', function () {

    it("Merge: should show clickable button",function(){
        expect("other test").toBe("uncommented");
    });
    return;

    var p = new Page();

    browser.ignoreSynchronization = true;

    it("should show software info",function(){

        p.get();

        browser.ignoreSynchronization = true;
        expect(p.versionNode.getText()).toMatch(/\d+\.\d+\.\d+.\d+/);//2.3.0.1234
        expect(p.archNode.getText()).toMatch(/[\w\d]+/);;//x86,arm,x64
        expect(p.platformNode.getText()).toMatch(/\w+/); //linux,mac,windows
    });


    it("should show storage info",function(){

        browser.ignoreSynchronization = true;
        expect(p.storagesNodes.count()).toBeGreaterThan(0); // storages exist
        expect(p.storagesNodes.first().element(by.css(".storage-url")).getText()).toMatch(/[\w\d/\\]+/); // Except one good url
        expect(p.storagesNodes.first().element(by.css(".storage-total-space")).getText()).toMatch(/[\d+\.?\d*\s*\wB]+/);// GB,  TB  - it's size
        expect(p.storagesNodes.first().element(by.css(".storage-indicators")).element(by.css(".glyphicon")).isDisplayed()).toBe(true); // At least one indicator
    });

    it("health monitoring: should display legend",function(){
        browser.ignoreSynchronization = true;
        expect(p.hmLegentNodes.count()).toBeGreaterThan(1); //legend exists, except 2+ elements
    });

    it("health monitoring: should update graphics, should show message when server is offline, should enable-disable indicators using checkboxes",function(){
        expect("health monitoring").toBe("tested manually");
    });

    it("log: should show iframe",function(){
        browser.ignoreSynchronization = true;
        expect(p.logIframe.isDisplayed()).toBe(true);
        expect(p.logIframe.getAttribute("src")).toMatch("/api/showLog");
    });

    it("log: should show refresh link to update iframe",function(){
        browser.ignoreSynchronization = true;
        expect(p.refreshLogButton.isDisplayed()).toBe(true);
        expect(p.refreshLogButton.getText()).toEqual("Refresh");
        expect(p.refreshLogButton.getAttribute("target")).toEqual(p.logIframe.getAttribute("name"));
        expect(p.refreshLogButton.getAttribute("href")).toEqual(p.logIframe.getAttribute("src"));
    });

    it("log: should have a link to open log in new window",function(){
        browser.ignoreSynchronization = true;
        expect(p.openLogButton.isDisplayed()).toBe(true);
        expect(p.openLogButton.getText()).toEqual("Open in a new window");
        expect(p.openLogButton.getAttribute("target")).toEqual("_blank");
        expect(p.openLogButton.getAttribute("href")).toMatch("/api/showLog");
    });

    browser.ignoreSynchronization = false;
});