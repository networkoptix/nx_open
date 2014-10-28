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

    it("should show warning tip",function(){
        expect(p.dangerAlert.isDisplayed()).toBe(true);
    });

    it("should allow user to change settings: limit, enabled",function(){
        var storage = p.storagesRows.first();
        expect(storage.element(by.model("storage.reservedSpaceGb")).isEnabled()).toBe(true);
        expect(storage.element(by.model("storage.isUsedForWriting")).isDisplayed()).toBe(true);
    });

    it("should validate GB value",function(){
        p.setStorageLimit('');
        expect(p.saveButton.isEnabled()).toBe(false);
        expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toMatch("has-error");

        p.setStorageLimit(5);
        expect(p.saveButton.isEnabled()).toBe(true);
        expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toNotMatch("has-error");

        p.setStorageLimit('bad value');
        expect(p.saveButton.isEnabled()).toBe(false);
        expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toMatch("has-error");

        p.setStorageLimit(5);
        expect(p.saveButton.isEnabled()).toBe(true);
        expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toNotMatch("has-error");

        p.setStorageLimit(1000000);
        expect(p.saveButton.isEnabled()).toBe(false);
        expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toMatch("has-error");

        p.setStorageLimit(5);
        expect(p.saveButton.isEnabled()).toBe(true);
        expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toNotMatch("has-error");

    });

    it("should forbid disabling all storages",function(){
        var total = p.storagesRows.count();
        for(var i=0;i<total;i++){
            var row = p.storagesRows.get(i);
            var checkbox = row.element(by.model("storage.isUsedForWriting"));

            var isSelected  = false;
            checkbox.isSelected().then(function(val){ isSelected = val; });
            if(isSelected)
                checkbox.click();

            expect(checkbox.isSelected()).toBe(false);
        }
        expect(p.saveButton.isEnabled()).toBe(false);
        expect("some warning").toBe("showed");
    });

    it("should show warning, then limit is more than free space",function(){
        var max = 0;
        p.storageLimitInput.getAttribute("max").then(function(val){max=val;});

        p.setStorageLimit(max.toFixed(0)-1);

        console.log("set max");
        console.log(max.toFixed(0)-1);

        expect(p.saveButton.isEnabled()).toBe(true);
        expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toNotMatch("has-warning");

        // click
        // handle confirm
        // check it's text
        // decline

        expect("test").toBe("written");
    });

    it("should show error, then server returns error",function(){
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

    it("should upload some bad file and display an error",function(){
        expect("test").toBe("written");
    });



    it("should hide progress bar after uploading",function(){
        expect("test").toBe("written");
    });
});