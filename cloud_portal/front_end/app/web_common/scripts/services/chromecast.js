'use strict';

angular.module('nxCommon')
    .service('chromeCast', function () {
        var self = this;
        if(jscd.browser.toLowerCase() != 'chrome'){
            return;
        }
        if(chrome.cast){
            this.castPlayer = new CastPlayer();
            this.castPlayer.initializeCastPlayer();

            this.load = function(stream, mimeType){
                this.castPlayer.playerHandler.setStream(stream, mimeType);
            };
        }
    });