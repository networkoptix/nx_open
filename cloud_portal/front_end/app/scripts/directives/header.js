'use strict';

angular.module('cloudApp')
    .directive('header', function (dialogs) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/header.html',
            scope:{
            },
            link:function(scope,element,attrs){
                scope.login = function(){
                    dialogs.login();
                }
            }
        };
    });
