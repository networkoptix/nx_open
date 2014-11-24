'use strict';

angular.module('webadminApp')
    .controller('SdkeulaCtrl', function ($scope) {
        $scope.agree = false;
        $scope.next = function(){
            window.open('nx_sdk-archive.zip');
            return false;
        };
    });
