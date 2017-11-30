'use strict';

angular.module('webadminApp')
    .controller('SdkeulaCtrl', ['$scope', '$routeParams', function ($scope, $routeParams) {
        $scope.agree = false;
        $scope.next = function(){
            if($routeParams.sdkFile == 'sdk' || $routeParams.sdkFile == 'storage_sdk' ){
                window.open($routeParams.sdkFile + '.zip');
            }
            return false;
        };
    }]);
