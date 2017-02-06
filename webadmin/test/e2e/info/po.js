'use strict';

var Page = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.version = element(by.cssContainingText('tr','Version')).element(by.css('b'));
    this.architec = element(by.cssContainingText('tr','Architecture')).element(by.css('b'));
    this.platform = element(by.cssContainingText('tr','Platform')).element(by.css('b'));
    this.storagesNodes = element.all(by.repeater("storage in storages"));
    this.graphCanvas = element(by.css('canvas'));
    this.legendCheckboxes = element.all(by.css('.legend-checkbox'));

    // need legendCheckboxFirst to verify that legend is loaded
    this.legendCheckboxFirst = element(by.id('legend-checkbox2'));
    this.hmLegendNodes = element.all(by.repeater("dataset in datasets"));
    this.logIframe = element(by.css("iframe.log-frame"));
    this.logIframeMore = element(by.css("iframe.log-frame-big"));
    this.refreshLogButton = element(by.id("refresh-log"));
    this.logInNewWindow = element(by.linkText('Open in new window'));
    this.openLogButton = element(by.id("open-log"));

    this.offlineAlert = element(by.css('.alert-danger')).$('b');

    this.get = function () {
        browser.get('/#/info');
    };
};
 
module.exports = Page;