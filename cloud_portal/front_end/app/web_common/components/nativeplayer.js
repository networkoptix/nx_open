(function () {
    
    'use strict';
    
    window.NativePlayer = function () {
        
        // requestAnimationFrame polyfill
        this.init = function (element, readyHandler) {
            this.video = element[0];
            this.readyHandler = readyHandler;
            this.readyHandler(this);
        };
    };
    
    window.NativePlayer.prototype.play = function () {
        var playPromise = this.video.play();
        if (playPromise) {
            playPromise.catch(function (error) {
                var errorString = error.toString();
                //We use a string because chrome does not return an object with things such as code and name
                if (error.name !== 'AbortError' && errorString.indexOf('pause') < 0 && errorString.indexOf('load') < 0) {
                    throw error;
                }
            });
        }
    };
    
    window.NativePlayer.prototype.pause = function () {
        this.video.pause();
    };
    
    window.NativePlayer.prototype.addEventListener = function (event, handler) {
        this.handlers = this.handlers || {};
        if (this.video && this.handlers[event] !== handler) {
            this.video.addEventListener(event, handler);
            this.handlers[event] = handler;
        }
    };
    
    window.NativePlayer.prototype.volume = function (volumeLevel) {
        this.video.volume = volumeLevel / 100;
    };
    
    window.NativePlayer.prototype.kill = function () {
        this.video.src = '';
    };
    
    window.NativePlayer.prototype.load = function (url, type) {
        this.video.setAttribute('type', type);
        this.video.src = url;
    };
})();
