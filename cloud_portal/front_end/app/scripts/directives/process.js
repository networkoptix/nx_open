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
                alertTimeout:'@'
            },
            link:function(scope,element,attrs){
                scope.attrs = attrs;
                scope.shown = {};

                if(typeof(scope.alertTimeout)=='undefined') {
                    scope.alertTimeout = Config.alertTimeout;
                }

                if(scope.alertTimeout === 0 || scope.alertTimeout === '0'){
                    scope.alertTimeout = 365*24*3600*1000; // long enough )
                }


                scope.alertTimeout2 = scope.alertTimeout;

                scope.closeAlert = function(type){
                    scope.shown[type] = true;
                };

                scope.$watch('process.processing',function(){
                    scope.shown = {};
                });
            }
        };
    }).directive('processButton', ['$timeout',function ($timeout) {
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

                function setFocusToInvalid(form){
                    $timeout(function() {
                        $("[name='" + form.$name + "']").find('.ng-invalid:visible:first').focus();
                    });
                }

                scope.attrs = attrs;
                scope.checkForm = function(){
                    if(scope.form && !scope.form.$valid){
                        //Set the form touched
                        touchForm(scope.form);
                        setFocusToInvalid(scope.form);
                        return false;
                    }else{
                        scope.process.run();
                    }
                }

            }
        };
    }]).directive('processLoading',function(){
        return {
            restrict: 'A',
            scope:{
                processLoading:'='
            },
            link:function(scope, element, attrs){
                function checkVisibility(){
                    if(scope.processLoading){
                        if(scope.processLoading.finished) {
                            element.removeClass('hidden');
                            element.removeClass('process-loading');
                            element.children('.preloader').remove();
                            return;
                        }
                        else if(scope.processLoading.processing){
                            element.removeClass('hidden');
                            element.addClass('process-loading');
                            if(!element.children('.preloader').get(0)) {
                                element.prepend('<div class="preloader"><img src="images/loader.gif"></div></div>');
                            }
                            return;
                        }
                    }
                    element.addClass('hidden');
                }

                scope.$watch('processLoading.finished',checkVisibility);
                scope.$watch('processLoading.processing',checkVisibility);
                checkVisibility();
            }
        }
    });