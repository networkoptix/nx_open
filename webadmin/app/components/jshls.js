'use strict';


var jshlsAPI = new (function () {
    'use strict';

    // requestAnimationFrame polyfill
    window.requestAnimationFrame = window.requestAnimationFrame || window.mozRequestAnimationFrame || window.webkitRequestAnimationFrame || window.msRequestAnimationFrame || setTimeout;

    this.init = function(element,readyHandler,errorHandler) {
        this.canvas = $("<canvas>").appendTo(element).get(0);
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

    this.pause = function(){
        if(currentVideo) {
            currentVideo.pause();
        }
    };

    this.load = function(url){

        // preconfiguration using <script>'s data-attributes values
        var worker = new Worker('components/mpegts/worker.js'),
            nextIndex = 0,
            sentVideos = 0,
            videos = [],
            lastOriginal,
            manifest = url,
            context = this.canvas.getContext('2d');

        // drawing new frame
        var self = this;
        function nextFrame() {
            if (currentVideo.paused || currentVideo.ended) {
                return;
            }
            context.drawImage(currentVideo, 0, 0);
            requestAnimationFrame(nextFrame);
        }

        worker.addEventListener('message', function (event) {
            var data = event.data, descriptor = '#' + data.index + ': ' + data.original;

            switch (data.type) {
                // worker is ready to convert
                case 'ready':
                    getMore();
                    return;

                // got debug message from worker
                case 'debug':
                    Function.prototype.apply.call(console[data.action], console, data.args);
                    return;

                // got new converted MP4 video data
                case 'video':
                    var video = document.createElement('video'), source = document.createElement('source');
                    source.type = 'video/mp4';
                    video.appendChild(source);

                    video.addEventListener('loadedmetadata', function () {
                        if (self.canvas.width !== this.videoWidth || self.canvas.height !== this.videoHeight) {
                            self.canvas.width = this.width = this.videoWidth;
                            self.canvas.height = this.height = this.videoHeight;
                        }
                    });

                    video.addEventListener('play', function () {
                        if (currentVideo !== this) {
                            if (!currentVideo) {
                                // UI initialization magic to be left in main HTML for unobtrusiveness
                                /*new Function(script.text).call({
                                    worker: worker,
                                    canvas: canvas,
                                    get currentVideo() { return currentVideo }

                                });*/
                            }
                            console.log('playing ' + descriptor);
                            currentVideo = this;
                            nextIndex++;
                            if (sentVideos - nextIndex <= 1) {
                                getMore();
                            }
                        }
                        nextFrame();
                    });

                    video.addEventListener('ended', function () {
                        delete videos[nextIndex - 1];
                        if (nextIndex in videos) {
                            videos[nextIndex].play();
                        }
                    });
                    if (video.src.slice(0, 5) === 'blob:') {
                        video.addEventListener('ended', function () {
                            URL.revokeObjectURL(this.src);
                        });
                    }

                    video.src = source.src = data.url;
                    video.load();

                    (function canplaythrough() {
                        console.log('converted ' + descriptor);
                        videos[data.index] = this;
                        if ((!currentVideo || currentVideo.ended) && data.index === nextIndex) {
                            this.play();
                        }
                    }).call(video);

                    return;
            }
        });

        // relative URL resolver
        var resolveURL = (function () {
            var doc = document,
                old_base = doc.getElementsByTagName('base')[0],
                old_href = old_base && old_base.href,
                doc_head = doc.head || doc.getElementsByTagName('head')[0],
                our_base = old_base || doc.createElement('base'),
                resolver = doc.createElement('a'),
                resolved_url;

            return function (base_url, url) {
                old_base || doc_head.appendChild(our_base);

                our_base.href = base_url;
                resolver.href = url;
                resolved_url  = resolver.href; // browser magic at work here

                old_base ? old_base.href = old_href : doc_head.removeChild(our_base);

                return resolved_url;
            };
        })();

        // loading more videos from manifest
        function getMore() {
            var ajax = new XMLHttpRequest();
            ajax.addEventListener('load', function () {
                console.log("loaded",manifest,this.responseText);
                // Here we have two options
                // #EXT-X-STREAM-INF - for new urls
                // #EXTINF:10.571 - for originals
                var originals =
                    this.responseText
                        .split(/\r?\n/)
                        .filter(RegExp.prototype.test.bind(/hls/))
                        .map(resolveURL.bind(null, manifest));

                /*if(originals.length == 0){
                    var newsources = this.responseText
                        .split(/\r?\n/)
                        .filter(RegExp.prototype.test.bind(/\.m3u8$/))
                        .map(resolveURL.bind(null, manifest));
                }*/
                originals = originals.slice(originals.lastIndexOf(lastOriginal) + 1);
                lastOriginal = originals[originals.length - 1];

                worker.postMessage(originals.map(function (url, index) {
                    return {url: url, index: sentVideos + index};
                }));

                sentVideos += originals.length;

                console.log('asked for ' + originals.length + ' more videos');
            });
            ajax.open('GET', manifest, true);
            ajax.send();
        }


    };
})();