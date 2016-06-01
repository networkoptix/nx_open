'use strict';

var Page = function () {
    this.get = function () {
        browser.get('/#/advanced');
    };

    this.dialog = element(by.css(".modal-dialog"));

    this.dialogButtonOk = element(by.buttonText("Ok"));

    this.dialogButtonCancel = element(by.buttonText("Cancel"));

    this.dialogButtonClose = element(by.buttonText("Close"));

    this.storagesRows = element.all(by.repeater("storage in storages"));
    this.storage = this.storagesRows.first();
    this.storageName = this.storage.element(by.css(".storage-url"));
    this.storagePartition = this.storage.element(by.css(".progress"));
    this.storageInternalIcon = this.storage.element(by.css("[title~=Internal]"));
    this.storageReservedInput = this.storage.element(by.model("storage.reservedSpaceGb"));
    this.storageIsUsedForWriting = this.storage.element(by.model("storage.isUsedForWriting"));

    this.isUsedCheckbox = element(by.model("storage.isUsedForWriting"));

    this.dangerAlert = element.all(by.css(".alert-danger")).first();

    this.saveStoragesButton = element(by.name("form")).element(by.buttonText("Save"));

    this.cancelButton = element(by.buttonText("Cancel"));

    this.selectWritableStorageAlert = element(by.id("selectWritableStorageAlert"));

    this.reduceArchiveAlert = element(by.id("reduceArchiveAlert"));

    this.storageLimitInput  = this.storagesRows.first().element(by.model("storage.reservedSpaceGb"));
    this.storageLimitInputParent  = this.storageLimitInput.element(by.xpath(".."));

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
    };

    this.upgradeButton = element(by.id("fileupload"));

    this.logLevelsForm = element(by.name("logLevels"));
    this.saveLogsButton = this.logLevelsForm.element(by.buttonText("Save"));
    this.mainLogLevelSelect = element(by.model('mainLogLevel'));
    this.mainLogLevelOptions = this.mainLogLevelSelect.$$('option');
    this.transLogLevelSelect = element(by.model('transLogLevel'));
    this.transLogLevelOptions = this.transLogLevelSelect.$$('option');
};

module.exports = Page;