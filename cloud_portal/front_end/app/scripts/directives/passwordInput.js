'use strict';

//http://stackoverflow.com/questions/28045427/angularjs-custom-directive-with-input-element-pass-validator-from-outside
angular.module('cloudApp')
    .directive('passwordInput', function () {
        return {
            restrict: 'E',
            templateUrl: 'views/components/password-input.html',
            require: "?ngModel",
            scope:{
                ngModel:'=',
                name:'=',
                id:'=',
                complex:'=',
                label:'='
            },
            link:function(scope,element,attrs){
                //Complex - check password complexity
                scope.Config = Config;
                scope.weakPassword = true;
                scope.$watch('ngModel',function(val){

                    if(!scope.passwordInput.password.$dirty || scope.passwordInput.password.$invalid){
                        scope.weakPassword = false;
                        return;
                    }

                    var strongRegex = new RegExp('^' + Config.passwordRequirements.strongRegex + '$');
                    scope.weakPassword = !strongRegex.test(scope.ngModel);
                });


            }
        };
    });