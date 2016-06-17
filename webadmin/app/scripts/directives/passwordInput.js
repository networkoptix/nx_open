'use strict';

//http://stackoverflow.com/questions/28045427/angularjs-custom-directive-with-input-element-pass-validator-from-outside
angular.module('webadminApp')
    .directive('passwordInput', ['mediaserver',function (mediaserver) {
        return {
            restrict: 'E',
            templateUrl: 'views/components/passwordInput.html',
            require: "ngModel",
            scope:{
                ngModel:'='
            },
            link:function(scope, element, attrs, ngModel){
                console.log("model:", scope.ngModel,'-', ngModel);
                scope.Config = Config;
                scope.weakPassword = true;
                function loadCommonPasswords(){
                    if(!Config.commonPasswordsList) {
                        mediaserver.getCommonPasswords().then(function (data) {
                            Config.commonPasswordsList = data.data;
                            checkCommonPassword();
                        });
                        return false;
                    }
                    return true;
                }
                loadCommonPasswords(); // Load passwords online
                function checkCommonPassword(){
                    if(!scope.ngModel){
                        return;
                    }
                    if(!loadCommonPasswords()){
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