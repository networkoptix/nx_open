'use strict';
var Page = require('./po.js');
describe('Information Page', function () {

    var p = new Page();

    beforeAll(function(){
        p.get();
    });

    it("should show software info",function(){
        expect(p.version.getText()).toMatch(/\d\.\d\.\d.\d+/);//2.3.0.1234
        expect(p.architec.getText()).toMatch(/\bx86|arm|x64\b|/);//x86,arm,x64
        expect(p.architec.getText()).toMatch(/[\w\d]+/);//x86,arm,x64
        expect(p.platform.getText()).toMatch(/\blinux|mac|windows\b/); //linux,mac,windows
        expect(p.platform.getText()).toMatch(/\w+/); //linux,mac,windows
    });

    it("should show storage info",function(){
        p.helper.getTab('Storage').click();
        expect(p.storagesNodes.count()).toBeGreaterThan(0); // storages exist

        p.storagesNodes.each( function(storage) {
            expect(storage.element(by.css(".storage-url")).getText()).toMatch(/[\w\d/\\]+/); // Except one good url
            expect(storage.element(by.css(".storage-total-space")).getText()).toMatch(/[\d+\.?\d*\s*\wB]+/);// GB,  TB  - it's size
            expect(storage.element(by.css(".storage-indicators")).element(by.css("[title~=Internal]")).isDisplayed()).toBe(true); // At least one indicator
        });
    });

    it("health monitoring: should display legend",function() {
        p.helper.getTab('Health monitoring').click();

        p.helper.waitIfNotPresent( p.legendCheckboxFirst, 3000);

        p.helper.ignoreSyncFor( function() {
            p.legendCheckboxes.each(function(checkbox){
                expect(checkbox.isPresent()).toBe(true);
            });
            expect(p.hmLegendNodes.count()).toBeGreaterThan(1); //legend exists, except 2+ elements
        });
    });

    it("Health monitoring: canvas is displayed",function(){
        p.helper.getTab('Health monitoring').click();
        browser.sleep(500);
        p.helper.ignoreSyncFor( function() {
            expect(p.graphCanvas.isDisplayed()).toBe(true);
            expect(p.graphCanvas.getAttribute('width')).toBeGreaterThan(100);
            expect(p.graphCanvas.getAttribute('height')).toBeGreaterThan(100);
        });
    });

    it("Health monitoring: message when server is offline",function(){
        p.helper.getTab('Health monitoring').click();

        p.helper.ignoreSyncFor( function() {
            expect(p.offlineAlert.getInnerHtml()).toContain('Server is offline now!');
        });
    });

    it("Piece of log is displayed in iframe",function(){
        p.helper.getTab('Log').click();

        browser.refresh();
        p.helper.getTab('Log').click();
        expect(p.logIframe.isDisplayed()).toBe(true);
        expect(p.logIframe.getAttribute("src")).toMatch("/api/showLog");
    });

    it("Log: Button Open in new window works - opens /web/api/showLog?lines=1000",function(){
        p.helper.getTab('Log').click();
        p.logInNewWindow.click();
        p.helper.performAtSecondTab(function() {
            p.helper.ignoreSyncFor( function() {
                expect(browser.getCurrentUrl()).toContain('/web/api/showLog?lines=1000');
            });
            browser.close();
        });
    });

    it("Log can be opened in new window",function(){
        p.helper.getTab('Log').click();

        expect(p.openLogButton.isDisplayed()).toBe(true);
        expect(p.openLogButton.getText()).toEqual("Open in new window");
        expect(p.openLogButton.getAttribute("target")).toEqual("_blank");
        expect(p.openLogButton.getAttribute("href")).toMatch("/api/showLog");
    });

    it("Refresh link to update iframe works",function(){
        p.helper.getTab('Log').click();

        expect(p.refreshLogButton.isDisplayed()).toBe(true);
        expect(p.refreshLogButton.getText()).toEqual("Refresh");
        expect(p.refreshLogButton.getAttribute("target")).toEqual(p.logIframe.getAttribute("name"));
        expect(p.refreshLogButton.getAttribute("href")).toEqual(p.logIframe.getAttribute("src"));
        p.refreshLogButton.click();
    });
});