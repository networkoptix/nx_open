'use strict';

var Page = function () {
    var self = this;
    this.get = function () {
        browser.get('/#/advanced');
    };
    this.dangerAlert = element.all(by.css('.alert-danger')).first();

    this.dialog = element(by.css('.modal-dialog'));
    this.dialogOkButton = element(by.buttonText('Ok'));
    this.dialogCancelButton = element(by.buttonText('Cancel'));
    this.dialogCloseButton = element(by.buttonText('Close'));

    this.storagesRows = element.all(by.repeater('storage in storages'));
    this.storage = this.storagesRows.first();
    this.storageName = this.storage.element(by.css('.storage-url'));
    this.storagePartition = this.storage.element(by.css('.progress'));
    this.storageInternalIcon = this.storage.element(by.css('[title~=Internal]'));
    this.storageReservedInput = this.storage.element(by.model('storage.reservedSpaceGb'));
    this.storageIsUsedForWriting = this.storage.element(by.model('storage.isUsedForWriting'));
    this.saveStoragesButton = element(by.name('form')).element(by.buttonText('Save'));
    this.cancelStoragesButton =element(by.name('form')). element(by.buttonText('Cancel'));

    this.storageLimitInput  = this.storagesRows.first().element(by.model('storage.reservedSpaceGb'));
    this.storageLimitInputParent  = this.storageLimitInput.element(by.xpath('..'));

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

    this.isUsedCheckbox = element(by.model('storage.isUsedForWriting'));

    this.selectWritableStorageAlert = element(by.id('selectWritableStorageAlert'));

    this.reduceArchiveAlert = element(by.id('reduceArchiveAlert'));

    this.upgradeButton = element(by.id('fileupload'));

    this.saveLogsButton = element(by.name("logLevels")).element(by.buttonText('Save'));
    this.mainLogLevelSelect = element(by.model('mainLogLevel'));
    this.mainLogLevelOptions = this.mainLogLevelSelect.$$('option');
    this.transLogLevelSelect = element(by.model('transLogLevel'));
    this.transLogLevelOptions = this.transLogLevelSelect.$$('option');

    this.sysSettings = element(by.name('systemSettings'));
    this.sysSettingsSaveButton = this.sysSettings.element(by.buttonText('Save'));
    this.sysSettingsCheckboxes = this.sysSettings.all(by.css('[type=checkbox]'));
    this.sysSettingsTextInputs = this.sysSettings.all(by.css('[type=text]'));
    this.sysSettingsNumInputs = this.sysSettings.all(by.css('[type=number]'));
    this.saveSysSettings = function () {
        this.sysSettingsSaveButton.click();
        // If confirmation alert appears on save, close it
        this.dialog.isPresent().then(function (isPresent) {
            if (isPresent) self.dialogCloseButton.click();
        });
    }
};

module.exports = Page;