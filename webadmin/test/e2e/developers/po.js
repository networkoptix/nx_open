'use strict';

var Page = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.apiLink = element(by.id("api-doc"));
    this.sdkLinkVideo = element(by.id("sdk-video"));
    this.sdkLinkStorage = element(by.id("sdk-storage"));

    this.sdkDownloadButton = element(by.buttonText("Next"));
    this.acceptEulaCheckbox = element(by.model("agree"));

    this.get = function () {
        browser.get('/#/developers');
    };

    this.body = element(by.css('body'));
};

module.exports = Page;