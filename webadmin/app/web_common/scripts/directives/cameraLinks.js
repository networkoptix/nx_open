/**
 * Created by evgeniybalashov on 06/02/17.
 */


angular.module('nxCommon')
    .directive('cameraLinks', ['mediaserver', function (mediaserver) {
        return {
            restrict: 'E',
            scope: {
                activeCamera: "=",
                position: "=",
                liveMode: "=",
                player: "=",
                currentResolution: "="
            },
            templateUrl: Config.viewsDirCommon + 'components/cameraLinks.html',
            link: function (scope, element/*, attrs*/) {
                scope.position = 0;
                scope.duration = 5*60;
                scope.resolution = '640p';
                scope.useAuth = true;

                scope.streamName = function(stream){
                    switch(stream.encoderIndex){
                        case 0:
                            return L.cameraLinks.highStream;
                        case 1:
                            return L.cameraLinks.lowStream;
                        case -1:
                            return L.cameraLinks.transcoding;
                    }
                    return L.cameraLinks.unknown;
                };

                scope.formatLink = function(camera, stream,transport){
                    var linkTemplates = {
                        'preview': 'http://{{host}}/api/image?physicalId={{physicalId}}{{previewPosition}}{{auth}}',
                        'hls':'http://{{host}}/hls/{{physicalId}}.m3u8?{{streamLetter}}{{position}}{{auth}}',
                        'rtsp':'rtsp://{{host}}/{{physicalId}}?stream={{streamIndex}}{{position}}{{auth}}',
                        'transrtsp':'rtsp://{{host}}/{{physicalId}}?stream={{streamIndex}}{{position}}&resolution={{resolution}}{{auth}}',
                        'webm':'http://{{host}}/media/{{physicalId}}.webm?pos={{position}}&resolution={{resolution}}{{auth}}',
                        'mjpeg':'http://{{host}}/media/{{physicalId}}.mpjpeg?pos={{position}}&resolution={{resolution}}{{auth}}',
                        'download':'http://{{host}}/hls/{{physicalId}}.mkv?{{streamLetter}}{{position}}&duration={{duration}}{{auth}}'
                    };

                    

                    return linkTemplates[transport].
                        replace("{{host}}", window.location.host).
                        replace("{{physicalId}}", camera.physicalId).
                        replace("{{streamIndex}}", stream).
                        replace("{{streamLetter}}", stream?'lo':'hi').
                        replace("{{auth}}", !scope.useAuth?'':'&auth=' + (transport=='rtsp'?mediaserver.authForRtsp():mediaserver.authForMedia())).
                        replace("{{auth}}", !scope.useAuth?'':'&auth=' + (mediaserver.authForMedia())).
                        replace("{{position}}", scope.liveMode || !scope.position?'':'&pos=' + scope.position).
                        replace("{{previewPosition}}", scope.liveMode || !scope.position?'&time=LATEST':'&time=' + scope.position).
                        replace("{{duration}}", scope.duration).
                        replace("{{resolution}}", scope.resolution);
                };

                scope.$watch("player",function(){scope.transport = scope.player == 'webm' ? 'webm' : 'hls'});

            }
        };
    }]);