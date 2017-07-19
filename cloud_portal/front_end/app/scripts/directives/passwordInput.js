'use strict';

//http://stackoverflow.com/questions/28045427/angularjs-custom-directive-with-input-element-pass-validator-from-outside
angular.module('cloudApp')
    .directive('passwordInput', ['cloudApi',function (cloudApi) {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/password-input.html',
            require: 'ngModel',
            scope:{
                ngModel:'=',
                name:'=',
                id:'=',
                complex:'=',
                label:'='
            },
            link:function(scope, element, attrs, ngModel){
                scope.Config = Config;
                scope.fairPassword = true;
                function loadCommonPasswords(){
                    if(!Config.commonPasswordsList) {
                        cloudApi.getCommonPasswords().then(function (data) {
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
                function checkComplexity(){
                    var classes = [
                        '[0-9]+',
                        '[a-z]+',
                        '[A-Z]+',
                        '[\\W_]+'
                    ];

                    var classesCount = 0;

                    for (var i = 0; i < classes.length; i++) {
                        var classRegex = classes[i];
                        if(new RegExp(classRegex).test(scope.ngModel)){
                            classesCount ++;
                        }
                    }
                    scope.passwordInput.password.$setValidity('weak', !scope.ngModel || classesCount >= Config.passwordRequirements.minClassesCount);

                    scope.fairPassword = classesCount < Config.passwordRequirements.strongClassesCount;

                }
                scope.$watch('ngModel',function(val){
                    checkCommonPassword();
                    checkComplexity();

                    if(!scope.passwordInput.password.$dirty || scope.passwordInput.password.$invalid){
                        scope.fairPassword = false;
                        return;
                    }
                });

            }
        };
    }]);