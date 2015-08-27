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
    var $currentVideo;

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
        if( currentVideo || node.has("video").length){
            node.find("video").remove();
        }
        $currentVideo = $("<video  src='" + url + "' type='" + type + "'>").appendTo(node).html('<source src="' + url + '" type="' + type + '"></source>' );

        currentVideo = $currentVideo.get(0);
        currentVideo.load();
    };
})();