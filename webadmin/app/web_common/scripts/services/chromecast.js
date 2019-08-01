(function() {

'use strict';

angular.module('nxCommon')
    .service('chromeCast', ['$location', function($location) {
        
        // Initialization of CastPlayer fails on non-secure connection
        if (jscd.browser.toLowerCase() !== 'chrome' || $location.protocol() !== 'https') {
            return;
        }
        if (chrome.cast) {
            try {
                this.castPlayer = new CastPlayer();
                this.castPlayer.initializeCastPlayer();
    
                this.load = function(stream, mimeType) {
                    this.castPlayer.playerHandler.setStream(stream, mimeType);
                };
            } catch (err) {
                console.error('CastPlayer failed with: ', err);
            }
        }
    }]);
})();
