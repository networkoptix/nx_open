'use strict';

angular.module('cloudApp')
    .directive('validateField', function () {
        return{
            restrict:'A',
            link:function(scope, element, attrs) {
                element = $(element);
                var input = element.find('input,textarea');
                if(!input.get(0)){ // No input inside - do nothing, just ignore directive
                    return;
                }

                var formName = element.attr('ng-form') || element.closest('form').attr('name');
                var fieldName = input.attr('name');

                var scopeName = formName + '.' + fieldName;

                scope.$watch(scopeName + '.$valid',updateValidity);
                scope.$watch(scopeName + '.$touched',updateValidity);

                function setUntouched(){
                    scope[formName][fieldName].$setUntouched();
                    scope.$apply();
                }
                input.on('keypress',setUntouched);
                input.on('cut',setUntouched);
                input.on('paste',setUntouched);
                input.on('keydown',function(){
                    var key = event.keyCode || event.charCode;
                    if( key == 8 || key == 46 ){
                        setUntouched();
                    }
                });

                function updateValidity(){
                    var touched = scope[formName][fieldName].$touched;
                    var valid = scope[formName][fieldName].$valid;
                    if(!touched){
                        element.removeClass('has-error');
                        return;
                    }
                    if(valid){
                        element.removeClass('has-error');
                        return;
                    }

                    element.addClass('has-error');
                }
                updateValidity();
            }
        }
    });