'use strict';


function NativePlayer(){

    // requestAnimationFrame polyfill
    this.init = function(element,readyHandler) {
        this.video = element[0];
        this.readyHandler = readyHandler;
        this.readyHandler(this);
    };
}

NativePlayer.prototype.play = function(){
    this.video.play().catch(function(error){
        var errorString = error.toString();
        if(error.name != 'AbortError' && errorString.indexOf('pause') < 0 && errorString.indexOf('load') < 0){
            console.log(errorString);
            throw error;
        }
    });
};

NativePlayer.prototype.pause = function(){
    this.video.pause();
};

NativePlayer.prototype.addEventListener = function(event,handler){
    this.handlers = this.handlers || {};
    if(this.video && this.handlers[event] !== handler){
        this.video.addEventListener(event,handler);
        this.handlers[event] = handler;
    }
};

NativePlayer.prototype.volume = function(volumeLevel){
    this.video.volume = volumeLevel/100;
}

NativePlayer.prototype.kill = function(){
    this.video.src = "";
};

NativePlayer.prototype.load = function(url, type){
    this.video.setAttribute('type', type);
    this.video.src = url;
};
