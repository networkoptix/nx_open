'use strict';

angular.module('cloudApp')
    .directive('header', function () {
        return {
            restrict: 'E',
            templateUrl: 'views/components/header.html',
            scope:{
            },
            link:function(scope,element,attrs){
            }
        };
    });
