/**
 * Created by evgeniybalashov on 06/02/17.
 */


angular.module('nxCommon')
    .filter('checkProtocol', function(){
        return function(url){
            return url.indexOf('//') >= 0 ? url : "//"+url;
        };
    })
    .directive('cameraLinks', ['systemAPI', function (systemAPI) {
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
                scope.debugMode = Config.allowDebugMode;
                
                var systemAPI = scope.system;
                scope.linkSettings = {
                    duration: 5*60,
                    resolution: '640p',
                    useAuth: false,
                    useCredentials: false,
                    useLogin: 'login',
                    usePassword: 'password'
                }
                scope.L = L;

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
                        'webm':'http://{{credentials}}{{host}}/media/{{cameraId}}.webm?resolution={{resolution}}{{position}}{{auth}}',
                        'mjpeg':'http://{{credentials}}{{host}}/media/{{cameraId}}.mpjpeg?resolution={{resolution}}{{position}}{{auth}}',
                        'download':'http://{{credentials}}{{host}}/hls/{{cameraId}}.mkv?{{streamLetter}}{{position}}&duration={{duration}}{{auth}}'
                    };

                    return linkTemplates[transport].
                        replace("{{credentials}}", scope.linkSettings.useCredentials?(scope.linkSettings.useLogin + ':' + scope.linkSettings.usePassword + '@'):'').
                        replace("{{host}}", systemAPI.apiHost()). // window.location.host
                        replace("{{cameraId}}", camera.id.replace('{','').replace('}','')).
                        replace("{{streamIndex}}", stream).
                        replace("{{streamLetter}}", stream?'lo':'hi').
                        replace("{{auth}}", !scope.linkSettings.useAuth?'':'&auth=' + (transport=='rtsp'?systemAPI.authPlay():systemAPI.authGet())).
                        replace("{{position}}", scope.liveMode || !scope.position?'':'&pos=' + Math.round(scope.position)).
                        replace("{{previewPosition}}", scope.liveMode || !scope.position?'&time=LATEST':'&time=' + Math.round(scope.position)).
                        replace("{{duration}}", scope.linkSettings.duration).
                        replace("{{resolution}}", scope.linkSettings.resolution);
                };

                scope.$watch("player",function(){scope.transport = scope.player == 'webm' ? 'WEBM' : 'HLS'});

            }
        };
    }]);