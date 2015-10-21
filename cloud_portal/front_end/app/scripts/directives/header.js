'use strict';

angular.module('cloudApp')
    .directive('header', function (dialogs, cloudApi) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/header.html',
            link:function(scope,element,attrs){
                scope.login = function(){
                    dialogs.login();
                };
                scope.logout = function(){
                    cloudApi.logout().then(function(){
                        window.location.reload();
                    });
                };

                cloudApi.account().then(function(account){
                    scope.account = account;
                });

            }
        };
    });
