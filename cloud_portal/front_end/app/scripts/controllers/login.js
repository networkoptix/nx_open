'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', ['$scope', 'cloudApi', 'process', '$location', '$routeParams', 'account',
        'dialogs', function ($scope, cloudApi, process, $location, $routeParams, account, dialogs) {

        $scope.Config = Config;
        var dialogSettings = dialogs.getSettings($scope);

        $scope.auth = {
            email: '',
            password: '',
            remember: true
        };

        $scope.close = function(){
            account.setEmail($scope.auth.email);

            dialogs.closeMe($scope);
        };

        $scope.login = process.init(function() {
            return account.login($scope.auth.email, $scope.auth.password, $scope.auth.remember);
        }).then(function(){
            if(dialogSettings.params.redirect){
                $location.path(Config.redirectAuthorised);
            }
            setTimeout(function(){
                document.location.reload();
            });
        });
    }]);
