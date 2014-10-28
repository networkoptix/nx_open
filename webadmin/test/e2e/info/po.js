'use strict';

var Page = function () {

    this.versionNode = element(by.id("software-version"));
    this.archNode = element(by.id("software-architecture"));
    this.platformNode = element(by.id("software-platform"));
    this.osNode = element(by.id("software-operatingsystem"));
    this.storagesNodes = element.all(by.repeater("storage in storages"));
    this.hmLegentNodes = element.all(by.repeater("dataset in datasets"));
    this.logIframe = element(by.css("iframe.log-frame"));
    this.refreshLogButton = element(by.id("refresh-log"));

    this.openLogButton = element(by.id("open-log"));


    this.get = function () {
        browser.get('/#/info');
    };
};

module.exports = Page;