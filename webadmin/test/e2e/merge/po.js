'use strict';

var Page = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.get = function () {
        browser.get('/#/settings');
    };
    this.mergeButton = element(by.buttonText("Merge Systems"));
    this.mergeDialog = element(by.id("mergeDialog"));

    this.dialogCloseButton = this.mergeDialog.element(by.css('.close'));

    this.systemSuggestionsList =  element(by.id("discoveredUrls"));
    this.urlInput = element(by.model("settings.url"));
    this.passwordInput = element(by.model("settings.password"));
    this.currentPasswordInput = element(by.model("settings.currentPassword"));

    this.findSystemButton = element(by.buttonText("Find system"));
    this.mergeSystemsButton = element(by.id("merge-systems-button"));

    this.currentSystemCheckbox = element(by.id("checkbox-current-system"));
    this.extarnalSystemCheckbox = element(by.id("checkbox-external-system"));
};

module.exports = Page;