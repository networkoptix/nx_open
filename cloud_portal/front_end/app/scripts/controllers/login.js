'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', ['$scope', 'cloudApi', 'process', '$location', '$localStorage', '$routeParams', 'account',
        'dialogs', function ($scope, cloudApi, process, $location, $localStorage, $routeParams, account, dialogs) {

        $scope.Config = Config;
        $scope.session = $localStorage;
        var dialogSettings = dialogs.getSettings($scope);

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
            if(dialogSettings.params.redirect){
                $location.path(Config.redirectAuthorised);
            }
            setTimeout(function(){
                document.location.reload();
            });
        });
    }]);
