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
                if(scope.actionType){
                    scope.buttonClass = 'btn-' + scope.actionType;
                } else {
                    scope.buttonClass = 'btn-primary';
                }

                function touchForm(form){

                    if (!angular.isObject(form)) {
                        form = scope[form] || null;
                    }

                    if (form) {
                        for (var control in form) {
                            if (form.hasOwnProperty(control) && control.indexOf('$') < 0) {
                                if (form[control].$submitted !== undefined) {  // Embedded form
                                    touchForm(form[control]);
                                } else {
                                    form[control].$setDirty();
                                    form[control].$setTouched();
                                }
                            }
                        }
                    }
                }

                function setFocusToInvalid(form){
                    $timeout(function() {
                        $('[name="' + form.$name + '"]').find('.ng-invalid:visible:first').focus();
                    });
                }

                scope.attrs = attrs;
                scope.checkForm = function(){
                    //Set the form touched
                    touchForm(scope.form);

                    $timeout(function () {
                        if(scope.form && (scope.form.$invalid || scope.form.$invalid === undefined)){
                            setFocusToInvalid(scope.form);
                            return false;
                        } else {
                            scope.process.run();
                        }
                    }, 0);
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
