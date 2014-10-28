'use strict';

var Page = function () {

    this.get = function () {
        browser.get('/#/help');
    };

    this.links = element.all(by.repeater("(key, link) in config.helpLinks"));
};

module.exports = Page;