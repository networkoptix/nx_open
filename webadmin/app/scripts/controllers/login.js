'use strict';

angular.module('webadminApp')
    .controller('LoginCtrl', function ($scope, auth) {
        $scope.login = function () {
            if ($scope.loginForm.$valid) {
                auth.login($scope.user);
            } else {
                $scope.loginForm.submitted = true;
            }
        };
    });
