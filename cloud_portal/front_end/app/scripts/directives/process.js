'use strict';

angular.module('cloudApp').directive('processAlert', function () {
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
                buttonDisabled:'=',
                buttonText:'=',
                form:'='
            },
            link:function(scope,element,attrs){

                function touchForm(form){
                    angular.forEach(form.$error, function (field) {
                        angular.forEach(field, function(errorField){
                            if(typeof(errorField.$touched) != 'undefined'){
                                errorField.$setTouched();
                            }else{
                                touchForm(errorField); // Embedded form - go recursive
                            }
                        })
                    });
                }
                scope.attrs = attrs;
                scope.checkForm = function(){
                    if(scope.form && !scope.form.$valid){
                        //Set the forum touched
                        touchForm(scope.form);
                        return false;
                    }else{
                        scope.process.run();
                    }
                }

            }
        };
    }).directive("processLoading",function(){
        return {
            restrict: 'A',
            scope:{
                processLoading:'='
            },
            link:function(scope, element, attrs){
                function checkVisibility(){
                    if(scope.processLoading && scope.processLoading.finished){
                        element.removeClass("process-loading");
                        element.children(".preloader").remove();
                    }else{

                        element.addClass("process-loading");

                        if(!element.children(".preloader").get(0)) {
                            element.prepend("<div class='preloader'><img src='images/loader.gif'></div></div>");
                        }
                    }
                }

                scope.$watch("processLoading.finished",checkVisibility);
                checkVisibility();
            }
        }
    });