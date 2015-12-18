'use strict';

angular.module('cloudApp')
    .directive('processAlert', function () {
        return {
            restrict: 'E',
            templateUrl: 'views/components/process-alert.html',
            scope:{
                process:'=',
                errorPrefix:'=',
                successMessage:'=',
                processMessage:'=',
                introMessage:'='
            },
            link:function(scope,element,attrs){
                scope.attrs = attrs;
            }
        };
    }).directive('processButton', function () {
        return {
            restrict: 'E',
            templateUrl: 'views/components/process-button.html',
            scope:{
                process:'=',
                disabled:'=',
                buttonText:'=',
                processingText:'='
            },
            link:function(scope,element,attrs){
                scope.attrs = attrs;
            }
        };
    });