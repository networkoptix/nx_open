'use strict';

angular.module('nxCommon')
    .service('chromeCast', function () {
        var self = this;
        this.castPlayer = new CastPlayer();
        this.castPlayer.initializeCastPlayer();

        this.load = function(stream, mimeType){
            this.castPlayer.playerHandler.setStream(stream, mimeType);
        };
    });