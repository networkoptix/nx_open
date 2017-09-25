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

                scope.formatLink = function(camera, stream, transport){
                    var linkTemplates = {
                        'preview': '{{protocol}}//{{credentials}}{{host}}/ec2/cameraThumbnail?cameraId={{cameraId}}{{previewPosition}}{{auth}}',
                        'hls':'{{protocol}}//{{credentials}}{{host}}/hls/{{cameraId}}.m3u8?{{streamLetter}}{{position}}{{auth}}',
                        'rtsp':'rtsp://{{credentials}}{{host}}/{{cameraId}}?stream={{streamIndex}}{{position}}{{auth}}',
                        'transrtsp':'rtsp://{{credentials}}{{host}}/{{cameraId}}?stream={{streamIndex}}{{position}}&resolution={{resolution}}{{auth}}',
                        'webm':'{{protocol}}//{{credentials}}{{host}}/media/{{cameraId}}.webm?resolution={{resolution}}{{position}}{{auth}}',
                        'mjpeg':'{{protocol}}//{{credentials}}{{host}}/media/{{cameraId}}.mpjpeg?resolution={{resolution}}{{position}}{{auth}}',
                        'download':'{{protocol}}//{{credentials}}{{host}}/hls/{{cameraId}}.mkv?{{streamLetter}}{{position}}&duration={{duration}}{{auth}}',
                        'web':'{{protocol}}//{{host}}{{webRootPath}}/view/{{cameraId}}?{{debug}}{{webPosition}}{{auth}}',
                    };

                    return linkTemplates[transport].
                        replace("{{protocol}}",window.location.protocol).
                        replace("{{credentials}}", scope.linkSettings.useCredentials?(scope.linkSettings.useLogin + ':' + scope.linkSettings.usePassword + '@'):'').
                        replace("{{host}}", systemAPI.apiHost()). // window.location.host
                        replace("{{cameraId}}", camera.id.replace('{','').replace('}','')).
                        replace("{{streamIndex}}", stream).
                        replace("{{streamLetter}}", stream?'lo':'hi').
                        replace("{{auth}}", !scope.linkSettings.useAuth?'':'&auth=' + (transport=='rtsp'?systemAPI.authPlay():systemAPI.authGet())).
                        replace("{{position}}", scope.liveMode || !scope.position?'':'&pos=' + Math.round(scope.position)).
                        replace("{{previewPosition}}", scope.liveMode || !scope.position?'&time=LATEST':'&time=' + Math.round(scope.position)).
                        replace("{{webPosition}}", scope.liveMode || !scope.position?'':'time=' + Math.round(scope.position)).
                        replace("{{webRootPath}}", systemAPI.systemId? '/' : (window.location.pathname + '#')).
                        replace("{{debug}}", scope.debugMode? 'debug' : '').
                        replace("{{duration}}", scope.linkSettings.duration).
                        replace("{{resolution}}", scope.linkSettings.resolution);
                };

                scope.$watch("player",function(){scope.transport = scope.player == 'webm' ? 'WEBM' : 'HLS'});

            }
        };
    }]);