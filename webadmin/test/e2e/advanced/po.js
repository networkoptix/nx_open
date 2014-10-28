'use strict';

var Page = function () {
    this.get = function () {
        browser.get('/#/advanced');
    };

    this.storagesRows = element.all(by.repeater("storage in storages"));

    this.dangerAlert = element(by.id("alert-danger-page"));
    this.saveButton = element(by.buttonText("Save"));

    this.cancelButton = element(by.buttonText("Cancel"));

    this.selectWritableStorageAlert = element(by.id("selectWritableStorageAlert"));

    this.reduceArchiveAlert = element(by.id("reduceArchiveAlert"));

    this.storageLimitInput  = this.storagesRows.first().element(by.model("storage.reservedSpaceGb"));
    this.setStorageLimit = function (val){
        //Hack from https://github.com/angular/protractor/issues/562
        var length = 20;
        var backspaceSeries = '';
        for (var i = 0; i < length; i++) {
            backspaceSeries += protractor.Key.BACK_SPACE;
        }
        this.storageLimitInput.sendKeys(backspaceSeries);

        this.storageLimitInput.clear();

        this.storageLimitInput.sendKeys(val);
    }

    this.upgradeButton = element(by.id("fileupload"));
};

module.exports = Page;