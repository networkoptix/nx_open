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
                        resolveForm(scope,formName)[fieldName].$setUntouched();
                        scope.$apply();
                    },0);
                });

                function resolveForm(scope,formName){
                    if(!scope){
                        return; // undefined
                    }

                    if(formName.indexOf('.')<0){
                        return scope[formName];
                    }

                    var pointIndex = formName.indexOf('.');
                    return resolveForm(scope[formName.substr(0,pointIndex)], formName.substr(pointIndex + 1));
                }

                function updateValidity(){
                    var form = resolveForm(scope,formName);
                    var touched = form[fieldName].$touched;
                    var valid = form[fieldName].$valid;
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