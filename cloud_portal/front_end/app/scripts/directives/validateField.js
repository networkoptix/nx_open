'use strict';

angular.module('cloudApp')
    .directive('validateField', function () {
        return{
            restrict:'A',
            link:function(scope, element, attrs) {

                var formName = element.parents('form').attr('name');
                var fieldName = element.find('input,textarea').attr('name');

                var scopeName = formName + '.' + fieldName;

                scope.$watch(scopeName + ".$valid",updateValidity);
                scope.$watch(scopeName + ".$dirty",updateValidity);

                function updateValidity(){
                    var dirty = scope[formName][fieldName].$dirty;
                    var valid = scope[formName][fieldName].$valid;
                    if(!dirty){
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