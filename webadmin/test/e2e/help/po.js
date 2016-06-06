'use strict';

var Page = function () {

    this.get = function () {
        browser.get('/#/help');
    };

    this.links = element.all(by.repeater("(key, link) in config.helpLinks"));
    this.supportButton = element(by.linkText('get support'));
    this.calculatorButton = element(by.linkText('calculate'));
    this.androidStoreButton = element(by.linkText('Android Client'));
    this.appleStoreButton = element(by.linkText('iOS Client'));
    this.body = element(by.css('body'));
};

module.exports = Page;