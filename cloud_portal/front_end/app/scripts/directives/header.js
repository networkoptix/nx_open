'use strict';

angular.module('cloudApp')
    .directive('header', function (dialogs, cloudApi,$sessionStorage) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/header.html',
            link:function(scope,element,attrs){
                scope.session = $sessionStorage;
                scope.loginChecked = false;
                scope.login = function(){
                    dialogs.login();
                };
                scope.logout = function(){
                    cloudApi.logout().then(function(){
                        scope.session.password = ''; // TODO: get rid of password in session storage
                        window.location.reload();
                    });
                };

                cloudApi.account().then(function(account){
                    scope.account = account;
                    scope.loginChecked = true;
                });

            }
        };
    });
