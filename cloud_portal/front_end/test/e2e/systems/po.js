'use strict';

var SystemsListPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.url = '/#/systems';

    this.htmlBody = element(by.css('body'));
    this.systemsList = element.all(by.repeater('system in systems'));
    this.openInNxButton = element(by.buttonText('Open in Nx Witness'));
    this.notActivatedLabel = element(by.cssContainingText('.label-danger', 'not activated'));

};

module.exports = SystemsListPage;