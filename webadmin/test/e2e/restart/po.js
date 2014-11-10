'use strict';

var Page = function () {

    this.restartButton = element(by.buttonText("Restart"));

    this.restartDialog = element(by.id("restartDialog"));
    this.refreshButton = element(by.buttonText("Refresh"));
    this.progressBar = element(by.id("restartingProgress"));
    this.get = function(){
        browser.get('http://admin:123@192.168.56.101:9000/static/index.html#/settings');
    };
};

module.exports = Page;