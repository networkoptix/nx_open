'use strict';

angular.module('webadminApp')
    .controller('SdkeulaCtrl', function ($scope,$routeParams) {
        $scope.agree = false;
        $scope.next = function(){
            if($routeParams.sdkFile == 'sdk' || $routeParams.sdkFile == 'storage_sdk' ){
                window.open($routeParams.sdkFile + '.zip');
            }
            return false;
        };
    });
