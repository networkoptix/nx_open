'use strict';

//http://stackoverflow.com/questions/28045427/angularjs-custom-directive-with-input-element-pass-validator-from-outside
angular.module('cloudApp')
    .directive('passwordInput', ['cloudApi',function (cloudApi) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/password-input.html',
            require: "ngModel",
            scope:{
                ngModel:'=',
                name:'=',
                id:'=',
                complex:'=',
                label:'='
            },
            link:function(scope,element,attrs,ngModel){
                //Complex - check password complexity
                scope.Config = Config;
                scope.weakPassword = true;
                function loadCommonPasswords(){
                    cloudApi.getCommonPasswords().then(function(data){
                        Config.commonPasswordsList = data.data;
                        checkCommonPassword();
                    });
                }
                function checkCommonPassword(){
                    if(!scope.ngModel){
                        return;
                    }
                    if(!Config.commonPasswordsList){
                        loadCommonPasswords();
                        return;
                    }
                    // Check if password is directly in common list

                    var commonPassword = Config.commonPasswordsList[scope.ngModel];

                    if(!commonPassword){
                        // Check if password is in uppercase and it's lowercase value is in common list
                        commonPassword = scope.ngModel.toUpperCase() == scope.ngModel &&
                            Config.commonPasswordsList[scope.ngModel.toLowerCase()];
                    }

                    scope.passwordInput.password.$setValidity('common', !commonPassword);
                }
                scope.$watch('ngModel',function(val){

                    checkCommonPassword();

                    if(!scope.passwordInput.password.$dirty || scope.passwordInput.password.$invalid){
                        scope.weakPassword = false;
                        return;
                    }

                    scope.weakPassword = !Config.passwordRequirements.strongPasswordCheck(scope.ngModel);
                });

            }
        };
    }]);