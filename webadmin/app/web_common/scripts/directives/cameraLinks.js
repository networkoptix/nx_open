/**
 * Created by evgeniybalashov on 06/02/17.
 */


angular.module('nxCommon')
    .directive('cameraLinks', [function () {
        return {
            restrict: 'E',
            scope: {
                system: "=",
                activeCamera: "=",
                position: "=",
                liveMode: "=",
                player: "=",
                currentResolution: "="
            },
            templateUrl: Config.viewsDirCommon + 'components/cameraLinks.html',
            link: function (scope, element/*, attrs*/) {
                var systemAPI = scope.system;
                scope.position = 0;
                scope.duration = 5*60;
                scope.resolution = '640p';
                scope.useAuth = true;

                scope.streamName = function(stream){
                    switch(stream.encoderIndex){
                        case 0:
                            return L.common.cameraLinks.highStream;
                        case 1:
                            return L.common.cameraLinks.lowStream;
                        case -1:
                            return L.common.cameraLinks.transcoding;
                    }
                    return L.common.cameraLinks.unknown;
                };

                scope.formatLink = function(camera, stream,transport){
                    var linkTemplates = {
                        'preview': 'http://{{host}}/ec2/cameraThumbnail?physicalId={{physicalId}}{{previewPosition}}{{auth}}',
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
                        replace("{{auth}}", !scope.useAuth?'':'&auth=' + (transport=='rtsp'?systemAPI.authPlay():systemAPI.authGet())).
                        replace("{{auth}}", !scope.useAuth?'':'&auth=' + (systemAPI.authGet())).
                        replace("{{position}}", scope.liveMode || !scope.position?'':'&pos=' + scope.position).
                        replace("{{previewPosition}}", scope.liveMode || !scope.position?'&time=LATEST':'&time=' + scope.position).
                        replace("{{duration}}", scope.duration).
                        replace("{{resolution}}", scope.resolution);
                };

                scope.$watch("player",function(){scope.transport = scope.player == 'webm' ? 'webm' : 'hls'});

            }
        };
    }]);