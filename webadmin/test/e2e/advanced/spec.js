'use strict';
var Page = require('./po.js');
describe('Advanced Page', function () {

    var p = new Page();

    beforeAll(function() {
        p.get();
    });

    it("shows warning about server failure",function(){
        expect(p.dangerAlert.isDisplayed()).toBe(true);
    });

    it("should allow user to see storages: free space, limit, totalspace, enabled, some indicator",function(){
        expect(p.storagesRows.count()).toBeGreaterThan(0);
        expect(p.storageName.getText()).toMatch(/[\w\:]*([\w\s]*)+/);
        expect(p.storagePartition.getAttribute("title"))
            .toMatch(/Reserved\:\s\d+\.?\d*\s*\wB,\s+Free\:\s\d+\.?\d*\s*\wB,\s+Occupied\:\s\d+\.?\d*\s*\wB,\s+Total:\s\d+\.?\d*\s*\wB/);
        expect(p.storageInternalIcon.isDisplayed()).toBe(true);
    });

    it("allows user to change settings: limit, enabled",function(){
        expect(p.storageReservedInput.isEnabled()).toBe(true);
        expect(p.storageIsUsedForWriting.isDisplayed()).toBe(true);
    });

    it("validates Reserved storage value",function(){
        p.setStorageLimit('');
        expect(p.saveStoragesButton.isEnabled()).toBe(false);
        expect(p.storageLimitInputParent.getAttribute("class")).toMatch("has-error");

        p.setStorageLimit(5);
        expect(p.saveStoragesButton.isEnabled()).toBe(true);
        expect(p.storageLimitInputParent.getAttribute("class")).not.toMatch("has-error");

        p.setStorageLimit('bad value');
        expect(p.saveStoragesButton.isEnabled()).toBe(false);
        expect(p.storageLimitInputParent.getAttribute("class")).toMatch("has-error");

        p.setStorageLimit(5);
        expect(p.saveStoragesButton.isEnabled()).toBe(true);
        expect(p.storageLimitInputParent.getAttribute("class")).not.toMatch("has-error");

        p.setStorageLimit(1000000);
        expect(p.saveStoragesButton.isEnabled()).toBe(false);
        expect(p.storageLimitInputParent.getAttribute("class")).toMatch("has-error");

        p.setStorageLimit(5);
        expect(p.saveStoragesButton.isEnabled()).toBe(true);
        expect(p.storageLimitInputParent.getAttribute("class")).not.toMatch("has-error");
    });

    it("shows warning, when limit is more than free space",function(){
        expect(p.reduceArchiveAlert.isDisplayed()).toBe(false);
        p.storageLimitInput.getAttribute("max").then(function(max){

            var toset = parseFloat(max).toFixed(0)-1;
            p.setStorageLimit(toset);

            expect(p.saveStoragesButton.isEnabled()).toBe(true);
            expect(p.storageLimitInputParent.getAttribute("class")).toMatch("has-warning");
            expect(p.reduceArchiveAlert.isDisplayed()).toBe(true);

            p.saveStoragesButton.click().then(function(){
                expect(p.dialog.getText()).toContain("Set reserved space is greater than free space left. " +
                    "Possible partial remove of the video footage is expected. " +
                    "Do you want to continue?");
                p.dialogOkButton.click();
                p.dialogCloseButton.click();
            });
        });
    });

    it("saves settings and displays them after reload",function(){
        p.setStorageLimit(1);

        p.saveStoragesButton.click().then(function(){

            expect(p.dialog.getText()).toContain("Settings saved");
            p.dialog.isDisplayed().then(function (isDisplayed) {
                if (isDisplayed) p.dialogCloseButton.click();
            });

            p.get();

            expect(p.storageLimitInput.getAttribute("value")).toBe("1");

            p.setStorageLimit(5);

            p.saveStoragesButton.click().then(function(){
                expect(p.dialog.getText()).toContain("Settings saved");
                p.dialogCloseButton.click();

                p.get();
                expect(p.storageLimitInput.getAttribute("value")).toBe("5");
            });

        });
    });

    it("warns on disabling all storages",function(){
        expect(p.selectWritableStorageAlert.isDisplayed()).toBe(false);

        p.storagesRows.each( function (row) {
            var checkbox = row.element(p.isUsedCheckbox.locator());
            checkbox.isSelected().then( function(isSelected) {
                if(isSelected) {
                    checkbox.click();
                }
                expect(checkbox.isSelected()).toBe(false);
            });
        });

        expect(p.selectWritableStorageAlert.isDisplayed()).toBe(true);
        p.saveStoragesButton.click();
        p.dialog.isPresent().then( function (isPresent) {
            if (isPresent) {
                expect(p.dialog.getText()).toContain('No main storage drive was selected for writing -' +
                    ' video will not be recorded on this server. Do you want to continue?');
                p.dialogOkButton.click();
                expect(p.dialog.getText()).toContain('Settings saved');
                p.dialogCloseButton.click();
            }
        });

        browser.refresh();
        // Check that no storages are selected for writing after refresh
        p.storagesRows.each( function (row) {
            expect(row.element(p.isUsedCheckbox.locator()).isSelected()).toBe(false);
        });

        // Enable first storage for writing again
        p.storageIsUsedForWriting.click();
        p.saveStoragesButton.click();
        p.dialog.isPresent().then( function (isPresent) {
            if (isPresent) {
                p.dialogCloseButton.click();
            }
        });
    });

    it("allows canceling changes",function(){
        p.storageLimitInput.getAttribute("value").then(function(oldlimit){
            p.setStorageLimit(1);
            expect(p.storageLimitInput.getAttribute("value")).toBe('1');
            p.cancelStoragesButton.click();
            expect(p.storageLimitInput.getAttribute("value")).toBe(oldlimit);
        });
    });

    // Log section tests

    it("Main log level select works and is saved",function(){
        p.mainLogLevelOptions.each(function(elem) {
            expect(elem.isSelected()).toBe(false);

            p.mainLogLevelSelect.isPresent().then( function(isPresent) {
                if(isPresent) {
                    p.mainLogLevelSelect.click();
                }
                else {
                    browser.sleep(500);
                }
            });
            elem.isDisplayed().then( function(isDisplayed) { // if the dropdown has opened
                if(isDisplayed) {
                    elem.click();
                    p.saveLogsButton.click();
                }
            });
            // If confirmation alert appears, close it
            p.dialog.isPresent().then( function(isPresent) {
                if(isPresent) {
                    p.dialogCloseButton.click();
                }
            });

            expect(elem.isSelected()).toBe(true);
        })
    });

    it("Transaction log level select works and is saved",function(){
        p.transLogLevelOptions.each(function(elem) {
            expect(elem.isSelected()).toBe(false);

            p.transLogLevelSelect.isPresent().then( function(isPresent) {
                if(isPresent) {
                    p.transLogLevelSelect.click();
                }
                else {
                    browser.sleep(500);
                }
            });
            elem.isDisplayed().then( function(isDisplayed) { // if the dropdown has opened
                if(isDisplayed) {
                    elem.click();
                    p.saveLogsButton.click();
                }
            });
            // If confirmation alert appears, close it
            p.dialog.isPresent().then( function(isPresent) {
                if(isPresent) {
                    p.dialogCloseButton.click();
                }
            });

            expect(elem.isSelected()).toBe(true);
        })
    });

    // Manual update tests
    it("shows button for manual updating system",function(){
        expect(p.upgradeButton.isEnabled()).toBe(true);
    });

    it("allows to upload some bad file and displays an error",function(){
        var path = require('path');

        var fileToUpload = './po.js';
        var absolutePath = path.resolve(__dirname, fileToUpload);

        p.upgradeButton.sendKeys(absolutePath).then(function(){

            expect(p.dialog.getText()).toContain("Updating failed");
            p.dialogCloseButton.click();
        });
    });

    //Advanced system settings
    it("All checkboxes are changeable; after save changes are applied",function(){
        var oldSysSettings = []; // array to store initial state of checkboxes

        // We do not want to toggle new system checkbox, so filtering it out
        p.sysSettingsCheckboxes.filter( function(checkbox) {
            return checkbox.getAttribute('id').then( function(id) {
                return id != 'newSystem';
            });
        }).each( function(checkbox) {
            checkbox.isSelected().then( function(wasSelected) {
                checkbox.click(); // toggle checkbox state

                if (wasSelected) expect(checkbox.isSelected()).toBe(false);
                else expect(checkbox.isSelected()).toBe(true);

                oldSysSettings.push(wasSelected); // filling array of initial states of checkboxes
            });
        });

        p.saveSysSettings();

        browser.refresh();
        browser.waitForAngular();

        // Grab new states of all checkboxes except newSystem, into array
        var newSysSettings = p.sysSettingsCheckboxes.filter( function(checkbox) {
            return checkbox.getAttribute('id').then( function(id) {
                return id != 'newSystem';
            });
        }).map( function(checkbox) {
            return checkbox.isSelected().then( function(isSelected) {
                return isSelected;
            });
        });

        // Check that state of all checkboxes has changed
        // by comparing two arrays
        for (var i=0; i < oldSysSettings.length; i++) {
            expect(oldSysSettings[i]).toBe(!newSysSettings[i]);
        }

        // Toggling all settings back to initial state
        // We do not want to toggle new system checkbox, so filtering it out
        p.sysSettingsCheckboxes.filter( function(checkbox) {
            return checkbox.getAttribute('id').then( function(id) {
                return id != 'newSystem';
            });
        }).each( function(checkbox) {
            checkbox.click(); // toggle checkbox state
        });

        p.saveSysSettings();
    });

    it("All number input fields are changeable; after save changes are applied",function(){
        var initialSettings = [];

        p.sysSettingsNumInputs.each( function(field) {
            field.getAttribute('value').then(function(value) {
                initialSettings.push(value);
            });
            field.sendKeys('5');
            expect(field.getAttribute('value')).toContain('5');
        });

        p.saveSysSettings();

        browser.refresh();
        browser.waitForAngular();

        p.sysSettingsNumInputs.each( function(field, index) {
            // Check that settings are saved after refresh
            expect(field.getAttribute('value')).toContain('5');
            // Restore initial settings
            field.clear()
                .sendKeys(initialSettings[index]);
        });
        p.saveSysSettings();
    });

    xit("All text input fields are changeable; after save changes are applied",function(){
        var initialSettings = p.sysSettingsTextInputs.map( function(field, index) {
            return field.getAttribute('value').then(function(value) {
                console.log(index, value);
                return value;
            })
        });

        p.sysSettingsTextInputs.each( function(field) {
            field.sendKeys('someAddedText');
            expect(field.getAttribute('value')).toContain('someAddedText');
        });

        p.saveSysSettings();
        //browser.pause();

        browser.refresh();
        browser.waitForAngular();

        p.sysSettingsTextInputs.each( function(field, index) {
            // Check that settings are saved after refresh
            expect(field.getAttribute('value')).toContain('someAddedText');
            // Restore initial settings
            field.clear()
                .sendKeys(initialSettings[index]);
        });
        p.saveSysSettings();
    });
});