'use strict';

angular.module('cloudApp').directive('processButton', ['$timeout',function ($timeout) {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/process-button.html',
            scope:{
                process:'=',
                buttonDisabled:'=',
                buttonText:'=',
                actionType:'=',
                form:'='
            },
            link:function(scope, element, attrs){
                scope.buttonClass = 'btn-primary';
                if(scope.actionType){
                    scope.buttonClass = 'btn-' + scope.actionType;
                }
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
                        $('[name="' + form.$name + '"]').find('.ng-invalid:visible:first').focus();
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
                element = $(element);
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
                                element.prepend('<div class="preloader"><img src="static/images/preloader.gif"></div>');
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