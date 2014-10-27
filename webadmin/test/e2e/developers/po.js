'use strict';

var Page = function () {

    this.apiLink = element(by.id("api-doc"));
    this.sdkLink = element(by.id("sdk-download"));

    this.sdkDownloadButton = element(by.buttonText("Next"));
    this.acceptEulaCheckbox = element(by.model("agree"));

    this.get = function () {
        browser.get('/#/developers');
    };
};

module.exports = Page;