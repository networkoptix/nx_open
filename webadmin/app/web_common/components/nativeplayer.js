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

    this.volume = function(volumeLevel){
        currentVideo.volume = volumeLevel/100;
    }

    this.kill = function(){
        this.destroy();
    };

    this.destroy = function(){
        if( currentVideo || node.has("video").length){
            try {
                node.find("video").attr("src", "");
                node.find("source").attr("src", "");
                node.find("video").get(0).load();
            }catch(a){
                // ignore errors above
            }
            node.find("video").remove();
        }
    };

    this.load = function(url,type){
        this.destroy();

        var videoClass = window.jscd.browser == 'Microsoft Internet Explorer'?'':'class="fullsize"';
        var mobileAttrs = window.jscd.mobile ? 'muted controls playsinline': '';
        $currentVideo = $('<video ' + videoClass + ' src="' + url + '" type="' + type + '"' + mobileAttrs+'><source src="' + url + '" type="' + type + '"></source></video>' ).appendTo(node);

        currentVideo = $currentVideo.get(0);

        currentVideo.load();


        /*var events = [
            "abort",// Data loading was aborted
            "error",// An error occured
            "emptied",// Data not present unexpectedly
            "stalled",// Data transfer stalled
            "pause",// Video has paused (fired with pause())
            "ended"//,// Video has ended

            //"loadstart",// Browser starts loading data
            //"progress",// Browser loads data
            //"suspend",// Browser does not load data, waiting
            //"play",// Video started playing (fired with play())
            //"loadedmetadata",// Metadata loaded
            //"loadeddata",// Data loaded
            //"waiting",// Waiting for more data
            //"playing",// Playback started
            //"canplay",// Video can be played, but possibly must stop to buffer content
            //"canplaythrough",// Enough data to play the whole video
            //"seeking",// seeking is true (browser seeks a position)
            //"seeked",// seeking is now false (position found)
            //"timeupdate",// currentTime was changed
            //"ratechange",// Playback rate has changed
            //"durationchange",// Duration has changed (for streams)
            //"volumechange"// Volume has changed"];
        ];
        for(var i=0;i<events.length;i++){
            (function(eventName) {
                currentVideo.addEventListener(eventName, function (event) {
                    try {
                        console.log("video event", eventName, event);
                    }catch(a){
                        // ignore errors
                    }
                });
            })(events[i]);
        }*/
    };
})();