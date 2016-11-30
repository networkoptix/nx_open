'use strict';

angular.module('webadminApp')
    .controller('MainCtrl', function ($scope, $location, nativeClient) {

        $scope.config = Config;

        if($location.path() == '/'){ // Do redirects
            nativeClient.init().then(function(mode){
                if(mode.lite){
                    $location.path("/client");
                    return;
                }
                $location.path("/settings");
            },function(){
                $location.path("/settings");
            });
        }
    });
