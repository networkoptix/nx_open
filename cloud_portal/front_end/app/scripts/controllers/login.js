'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', function ($scope) {

        $scope.email = '';
        $scope.password = '';

        $scope.login = function(){
            alert("cool");
        }
    });