'use strict';

var Page = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.versionNode = element(by.id("software-version"));
    this.archNode = element(by.id("software-architecture"));
    this.platformNode = element(by.id("software-platform"));
    this.storagesNodes = element.all(by.repeater("storage in storages"));
    this.graphCanvas = element(by.css('canvas'));
    this.legendCheckboxes = element.all(by.css('.legend-checkbox'));

    // need legendCheckboxFirst to verify that legend is loaded
    this.legendCheckboxFirst = element(by.id('legend-checkbox2'));
    this.hmLegendNodes = element.all(by.repeater("dataset in datasets"));
    this.logIframe = element(by.css("iframe.log-frame"));
    this.logIframeMore = element(by.css("iframe.log-frame-big"));
    this.refreshLogButton = element(by.id("refresh-log"));
    this.moreLogLinesButton = element(by.linkText('More lines'));
    this.openLogButton = element(by.id("open-log"));

    this.offlineAlert = element(by.css('.alert-danger')).$('b');

    this.get = function () {
        browser.get('/#/info');
    };

    this.ignoreSyncFor = function(callback) {
        browser.ignoreSynchronization = true;
        callback();
        browser.ignoreSynchronization = false;
    }
};
 
module.exports = Page;