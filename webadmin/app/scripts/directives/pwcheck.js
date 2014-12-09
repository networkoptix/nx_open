'use strict';

angular.module('webadminApp')
    .directive('nxEqualEx', function() {
        return {
            require: 'ngModel',
            link: function (scope, elem, attrs, model) {
                if (!attrs.nxEqualEx) {
                    console.error('nxEqualEx expects a model as an argument!');
                    return;
                }
                function updateValidity(){
                    // Only compare values if the second ctrl has a value.
                    if (model.$viewValue !== undefined && model.$viewValue !== '') {
                        model.$setValidity('nxEqualEx', scope.$eval(attrs.nxEqualEx) === model.$viewValue);
                    }
                }
                scope.$watch(attrs.nxEqualEx, updateValidity);
                scope.$watch('$viewValue', updateValidity);

                model.$parsers.push(function (value) {
                    // Mute the nxEqual error if the second ctrl is empty.
                    if (value === undefined || value === '') {
                        model.$setValidity('nxEqualEx', true);
                        return value;
                    }
                    var isValid = value === scope.$eval(attrs.nxEqualEx);
                    model.$setValidity('nxEqualEx', isValid);
                    return isValid ? value : undefined;
                });
            }
        };
    });
