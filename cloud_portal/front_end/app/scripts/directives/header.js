'use strict';

angular.module('cloudApp')
    .directive('header', function (dialogs) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/header.html',
            link:function(scope,element,attrs){
                scope.login = function(){
                    dialogs.login();
                }
            }
        };
    });
