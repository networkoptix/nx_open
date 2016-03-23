'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', function ($scope, cloudApi, process, $location, $sessionStorage, $routeParams, account, dialogs) {

        $scope.Config = Config;
        $scope.session = $sessionStorage;

        $scope.auth = {
            email: '',
            password: '',
            remember: true
        };

        $scope.close = function(){
            $scope.session.email = $scope.auth.email;

            dialogs.closeMe($scope);
        };

        $scope.login = process.init(function() {
            $scope.session.email = $scope.auth.email;
            return cloudApi.login($scope.auth.email, $scope.auth.password, $scope.auth.remember);
        }).then(function(){
            if($scope.auth.password) { // TODO: This is dirty security hole, but I need this to make "Open in client" work
                $scope.session.password = $scope.auth.password;
            }
            setTimeout(function(){
                document.location.reload();
            });
        });
    });
