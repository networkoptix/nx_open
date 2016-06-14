'use strict';

var Page = function () {

    this.restartButton = element(by.partialButtonText("Restart"));
    this.dialog = element(by.css(".modal-dialog"));
    this.dialogButtonOk = element(by.buttonText("Ok"));
    this.dialogButtonCancel = element(by.buttonText("Cancel"));
    this.dialogButtonClose = element(by.buttonText("Close"));
    this.restartDialog = element(by.id("restartDialog"));
    this.progressBar = element(by.id("restartingProgress"));

    this.get = function () {
        browser.get('/#/settings');
    };
};

module.exports = Page;