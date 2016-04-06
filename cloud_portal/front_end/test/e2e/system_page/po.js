'use strict';

var SystemPage = function () {
    var Helper = require('../helper.js');
    this.helper = new Helper();

    this.url = '/#/systems';
    this.userEmailOwner = 'noptixqa+owner@gmail.com';
    this.userEmailAdmin = 'noptixqa+admin@gmail.com';
    this.userEmailViewer = 'noptixqa+viewer@gmail.com';
    this.userEmailAdvViewer = 'noptixqa+advviewer@gmail.com';
    this.userEmailLiveViewer = 'noptixqa+liveviewer@gmail.com';
    this.userEmailNoPerm = 'noptixqa+noperm@gmail.com';
    this.systemsList = element.all(by.repeater('system in systems'));
    this.ownedSystem = element.all(by.cssContainingText('h2','kate ubuntu')).first();

    this.systemName = element(by.css('h1'));
    this.systemOwn = element.all(by.css('.panel')).get(0).element(by.css('h2')); // TODO: add better id here
    this.openInNxButton = element(by.buttonText('Open in Nx Witness'));
    this.shareButton = element(by.partialButtonText('Share'));

};

module.exports = SystemPage;