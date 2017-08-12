/**
 * Created by evgeniybalashov on 06/02/17.
 */


angular.module('nxCommon')
    .filter('checkProtocol', function(){
        return function(url){
            return url.indexOf('//') >= 0 ? url : "//"+url;
        };
    })
    .directive('cameraLinks', [function () {
        return {
            restrict: 'E',
            scope: {
                system: "=",
                activeCamera: "=",
                position: "=",
                liveMode: "=",
                player: "=",
                currentResolution: "=",
                cameraLinks: "="
            },
            templateUrl: Config.viewsDirCommon + 'components/cameraLinks.html',
            link: function (scope, element/*, attrs*/) {
                var systemAPI = scope.system;
                scope.position = 0;
                scope.duration = 5*60;
                scope.resolution = '640p';
                scope.useAuth = false;
                scope.useCredentials = false;
                scope.useLogin = 'login';
                scope.usePassword = 'password';

                scope.clipboardSupported = true; // presume

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
                        'preview': 'http://{{credentials}}{{host}}/ec2/cameraThumbnail?cameraId={{cameraId}}{{previewPosition}}{{auth}}',
                        'hls':'http://{{credentials}}{{host}}/hls/{{cameraId}}.m3u8?{{streamLetter}}{{position}}{{auth}}',
                        'rtsp':'rtsp://{{credentials}}{{host}}/{{cameraId}}?stream={{streamIndex}}{{position}}{{auth}}',
                        'transrtsp':'rtsp://{{credentials}}{{host}}/{{cameraId}}?stream={{streamIndex}}{{position}}&resolution={{resolution}}{{auth}}',
                        'webm':'http://{{credentials}}{{host}}/media/{{cameraId}}.webm?pos={{position}}&resolution={{resolution}}{{auth}}',
                        'mjpeg':'http://{{credentials}}{{host}}/media/{{cameraId}}.mpjpeg?pos={{position}}&resolution={{resolution}}{{auth}}',
                        'download':'http://{{credentials}}{{host}}/hls/{{cameraId}}.mkv?{{streamLetter}}{{position}}&duration={{duration}}{{auth}}'
                    };

                    return linkTemplates[transport].
                        replace("{{credentials}}", scope.useCredentials?(scope.useLogin + ':' + scope.usePassword):'').
                        replace("{{host}}", window.location.host).
                        replace("{{cameraId}}", camera.id.replace('{','').replace('}','')).
                        replace("{{streamIndex}}", stream).
                        replace("{{streamLetter}}", stream?'lo':'hi').
                        replace("{{auth}}", !scope.useAuth?'':'&auth=' + (transport=='rtsp'?systemAPI.authPlay():systemAPI.authGet())).
                        replace("{{auth}}", !scope.useAuth?'':'&auth=' + (systemAPI.authGet())).
                        replace("{{position}}", scope.liveMode || !scope.position?'':'&pos=' + Math.round(scope.position)).
                        replace("{{previewPosition}}", scope.liveMode || !scope.position?'&time=LATEST':'&time=' + Math.round(scope.position)).
                        replace("{{duration}}", scope.duration).
                        replace("{{resolution}}", scope.resolution);
                };

                scope.$watch("player",function(){scope.transport = scope.player == 'webm' ? 'WEBM' : 'HLS'});

            }
        };
    }]);