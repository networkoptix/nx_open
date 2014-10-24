'use strict';
var Page = require('./po.js');
describe('Advanced Page', function () {

    var p = new Page();

    it("should allow user to see storages: free space, limit, totalspace, enabled, some indicator",function(){
        p.get();
        expect(p.storagesRows.count()).toBeGreaterThan(0);
        var storage = p.storagesRows.first();
        expect(storage.element(by.css(".storage-url")).getText()).toMatch(/[\w\d\:]*(\/[\w\d]*)+/);
        expect(storage.element(by.css(".progress")).getAttribute("title")).toMatch(/Reserved\:\s\d+\.?\d*\s*\wB,\s+Free\:\s\d+\.?\d*\s*\wB,\s+Occupied\:\s\d+\.?\d*\s*\wB,\s+Total:\s\d+\.?\d*\s*\wB/);
        expect(storage.element(by.css(".glyphicon")).isDisplayed()).toBe(true);
    });

    it("should allow user to change settings: limit, enabled",function(){
        expect("test").toBe("written");
    });

    it("should show warnings in some cases",function(){
        expect("test").toBe("written");
    });

    it("should show confirm before saving",function(){
        expect("test").toBe("written");
    });


    it("cancel button should cancel changes",function(){
        expect("test").toBe("written");
    });

    it("should save settings and display it after reload",function(){
        expect("test").toBe("written");
    });

    it("should show button for updating system",function(){
        expect("test").toBe("written");
    });

    it("should upload some file and display an error",function(){
        expect("test").toBe("written");
    });


    it("Module",function(){
        expect("all tests").toBe("written");
    });

});