'use strict';

angular.module('webadminApp')
    .directive('validateField', function () {
        return{
            restrict:'A',
            link:function(scope, element/*, attrs*/) {
                var input = element.find('input,textarea');
                if(!input.get(0)){ // No input inside - do nothing, just ignore directive
                    return;
                }



                var formName = element.attr('ng-form') || element.closest('form').attr('name');
                var fieldName = input.attr('name');

                var scopeName = formName + '.' + fieldName;

                input.on('focus',function(){
                    setTimeout(function(){
                        scope[formName][fieldName].$setUntouched();
                        scope.$apply();
                    },0);
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

                scope.$watch(scopeName + '.$valid',updateValidity);
                scope.$watch(scopeName + '.$touched',updateValidity);

                updateValidity();
            }
        };
    });