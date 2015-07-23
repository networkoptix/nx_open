'use strict';


var nativePlayer = new (function () {
    'use strict';

    // requestAnimationFrame polyfill
    var node;

    this.init = function(element,readyHandler,errorHandler) {
        node = element;

        this.readyHandler = readyHandler;
        this.errorHandler = readyHandler;

        this.readyHandler(this);
    };

    var currentVideo;

    this.play = function(){
        if(currentVideo) {
            currentVideo.play();
        }
    };

    this.addEventListener = function(event,handler){
        console.log("try to subscribe",event,handler);
        if(currentVideo) {
            console.log("subscribe",event,handler);
            currentVideo.addEventListener(event,handler);
        }
    };
    this.pause = function(){
        if(currentVideo) {
            currentVideo.pause();
        }
    };

    this.load = function(url,type){
        console.log("load native ",url,type);
        currentVideo = $("<video controls src='" + url + /*"' type='" + type +*/ "'>").appendTo(node).get(0);
    };
})();