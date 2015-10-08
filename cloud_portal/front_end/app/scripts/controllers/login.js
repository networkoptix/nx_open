'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', function ($scope, $modalInstance) {
        $scope.login = function(){
            alert("cool");
        }
    });