'use strict';

angular.module('cloudApp')
    .directive('header', function (dialogs, cloudApi, $sessionStorage, account, $location) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/header.html',
            link:function(scope,element,attrs){
                scope.session = $sessionStorage;

                scope.inline = typeof($location.search().inline) != 'undefined';

                if(scope.inline){
                    $("body").addClass("inline-portal");
                }

                scope.login = function(){
                    dialogs.login();
                };
                scope.logout = function(){
                    cloudApi.logout().then(function(){
                        scope.session.password = ''; // TODO: get rid of password in session storage
                        window.location.reload();
                    });
                };

                account.get().then(function(account){
                    scope.account = account;
                    $("body").removeClass("loading");
                    $("body").addClass("authorized");
                },function(){
                    $("body").removeClass("loading");
                    $("body").addClass("anonymous");
                });
            }
        };
    });
