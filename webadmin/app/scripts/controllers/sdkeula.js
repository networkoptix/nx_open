'use strict';

angular.module('webadminApp')
    .controller('SdkeulaCtrl', function ($scope,$routeParams) {
        $scope.agree = false;
        $scope.next = function(){
            window.open($routeParams.sdkFile + '.zip');
            return false;
        };
    });
