'use strict';

var Page = function () {
    this.get = function () {
        browser.get('/#/advanced');
    };

    this.storagesRows = element.all(by.repeater("storage in storages"));
};

module.exports = Page;