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
        if(currentVideo) {
            currentVideo.addEventListener(event,handler);
        }
    };
    this.pause = function(){
        if(currentVideo) {
            currentVideo.pause();
        }
    };

    this.load = function(url,type){

        if(!currentVideo || !node.has("video").length) {
            currentVideo = $("<video  src='" + url + "' type='" + type + "'>").appendTo(node).get(0);
        }else{
            currentVideo.pause();
            $(currentVideo).attr("src",url).attr("type",type);
        }
    };
})();