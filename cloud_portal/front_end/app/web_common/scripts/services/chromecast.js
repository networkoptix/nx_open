'use strict';

angular.module('nxCommon')
    .service('chromeCast', ['$location', function ($location) {
        var self = this;
    
        // Check if page is displayed inside an iframe
        self.isEmbeded = ($location.path().indexOf('/embed') === 0);
        
        // Do not initialize Chrome cast if page is inside iframe or not supported
        if(jscd.browser.toLowerCase() !== 'chrome' || self.isEmbeded){
            return;
        }
        
        if(chrome.cast) {
            self.castPlayer = new CastPlayer();
            self.castPlayer.initializeCastPlayer();
    
            self.load = function (stream, mimeType) {
                this.castPlayer.playerHandler.setStream(stream, mimeType);
            };
        }
    }]);
