'use strict';

angular.module('webadminApp')
    .controller('MainCtrl', ['$scope', '$location', 'nativeClient', 'mediaserver',
    function ($scope, $location, nativeClient, mediaserver) {
        $scope.config = Config;

        if($location.path() == '/'){ // Do redirects
            nativeClient.init().then(function(mode){
                if(mode.lite){
                    $('body').addClass('lite-client-mode');
                    $location.path("/client");
                    return;
                }
                if(mediaserver.hasProxy()){
                    $location.path("/view");
                }else {
                    $location.path("/settings");
                }
            },function(){
                if(mediaserver.hasProxy()){
                    $location.path("/view");
                }else {
                    $location.path("/settings");
                }
            });
        }
    }]);
