'use strict';

angular.module('cloudApp')
    .controller('LoginCtrl', ['$scope', 'cloudApi', 'process', '$location', '$routeParams', 'account',
        'dialogs', function ($scope, cloudApi, process, $location, $routeParams, account, dialogs) {

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

        $scope.$on('$destroy', function(){
            dialogs.dismissNotifications();
        });

        $scope.login = process.init(function() {
            return account.login($scope.auth.email, $scope.auth.password, $scope.auth.remember);
        },{
            ignoreUnauthorized: true,
            errorCodes:{
                accountNotActivated: function(){
                    $location.path('/activate');
                    return false;
                }
            }
        }).then(function(){
            if(dialogSettings.params.redirect){
                $location.path($routeParams.next ? $routeParams.next : Config.redirectAuthorised);
            }
            setTimeout(function(){
                document.location.reload();
            });
        });
    }]);
