'use strict';
var Page = require('./po.js');
describe('Advanced Page', function () {



    //it("should stop test",function(){expect("other test").toBe("uncommented");});return;

    var p = new Page();
    var ptor = protractor.getInstance();

    it("should allow user to see storages: free space, limit, totalspace, enabled, some indicator",function(){
        p.get();
        expect(p.storagesRows.count()).toBeGreaterThan(0);
        var storage = p.storagesRows.first();
        expect(storage.element(by.css(".storage-url")).getText()).toMatch(/[\w\d\:]*(\/[\w\d]*)+/);
        expect(storage.element(by.css(".progress")).getAttribute("title")).toMatch(/Reserved\:\s\d+\.?\d*\s*\wB,\s+Free\:\s\d+\.?\d*\s*\wB,\s+Occupied\:\s\d+\.?\d*\s*\wB,\s+Total:\s\d+\.?\d*\s*\wB/);
        expect(storage.element(by.css(".glyphicon")).isDisplayed()).toBe(true);
    });


    it("should show reserved space value",function(){
        expect("test").toBe("written");
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

    it("should show warning, then limit is more than free space",function(){
        p.get();
        expect(p.reduceArchiveAlert.isDisplayed()).toBe(false);
        p.storageLimitInput.getAttribute("max").then(function(max){

            var toset = parseFloat(max).toFixed(0)-1;
            p.setStorageLimit(toset);

            expect(p.saveButton.isEnabled()).toBe(true);
            expect(p.storageLimitInput.element(by.xpath("..")).getAttribute("class")).toMatch("has-warning");
            expect(p.reduceArchiveAlert.isDisplayed()).toBe(true);
            p.saveButton.click().then(function(){
                var alertDialog = ptor.switchTo().alert();
                expect(alertDialog.getText()).toContain("Possible partial remove of the video footage is expected");
                alertDialog.dismiss();
            });
        });
    });

    it("should save settings and display it after reload",function(){
        p.get();
        p.setStorageLimit(1);

        p.saveButton.click().then(function(){
            var alertDialog = ptor.switchTo().alert();
            expect(alertDialog.getText()).toContain("Settings saved");
            alertDialog.accept();

            p.get();
            expect(p.storageLimitInput.getAttribute("value")).toBe("1");

            p.setStorageLimit(5);

            console.log("click 3");
            p.saveButton.click().then(function(){
                var alertDialog = ptor.switchTo().alert();
                expect(alertDialog.getText()).toContain("Settings saved");
                alertDialog.accept();

                p.get();
                expect(p.storageLimitInput.getAttribute("value")).toBe("5");
            });

        });
    });

    it("should forbid disabling all storages",function(){
        p.get();
        expect(p.selectWritableStorageAlert.isDisplayed()).toBe(false);

        var total = p.storagesRows.count();
        var d = protractor.promise.defer();
        var readycounter=0;
        for(var i=0;i<total;i++){
            console.log("test checkboxes",i);
            var row = p.storagesRows.get(i);
            var checkbox = row.element(by.css(".isUsedForWriting"));//by.model("storage.isUsedForWriting"));
            checkbox.isSelected().then(function(isSelected){

                if(isSelected) {
                    checkbox.click();
                    console.log("click item to deselect");
                }

                expect(checkbox.isSelected()).toBe(false);
                readycounter++;

                if(readycounter == total){
                     d.fulfill("ok");
                    done();
                }
            });
        }
        d.promise.then(function(){
            expect(p.selectWritableStorageAlert.isDisplayed()).toBe(true);
            expect(p.saveButton.isEnabled()).toBe(false);
        });
    });

    it("should allow cancel changes",function(){
        p.storageLimitInput.getAttribute("value").then(function(oldlimit){
            p.setStorageLimit(1);
            p.cancelButton.click();
            expect(p.storageLimitInput.getAttribute("value")).toBe(oldlimit);
        });
    });

    it("should show button for updating system",function(){
        expect(p.upgradeButton.isEnabled()).toBe(true);
    });

    it("should upload some bad file and display an error",function(){
        var path = require('path');

        var fileToUpload = './po.js';
        var absolutePath = path.resolve(__dirname, fileToUpload);
        p.upgradeButton.sendKeys(absolutePath).then(function(){
            var alertDialog = ptor.switchTo().alert();
            expect(alertDialog.getText()).toContain("Updating failed");
            alertDialog.accept();
        });
    });
});