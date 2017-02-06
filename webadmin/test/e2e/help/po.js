'use strict';

var Page = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.get = function () {
        browser.get('/#/help');
    };

    this.links = element.all(by.repeater("(key, link) in config.helpLinks"));
    this.supportLink = element(by.linkText('Get support'));
    this.calculatorLink = element(by.linkText('Go to the calculator'));
    this.androidStoreButton = element(by.linkText('Android Client'));
    this.appleStoreButton = element(by.linkText('iOS Client'));
    this.body = element(by.css('body'));
};

module.exports = Page;