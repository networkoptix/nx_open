'use strict';

angular.module('webadminApp')
    .directive('nxEqualEx',['$log', function($log) {
        return {
            require: 'ngModel',
            link: function (scope, elem, attrs, model) {
                if (!attrs.nxEqualEx) {
                    $log.error('nxEqualEx expects a model as an argument!');
                    return;
                }
                function updateValidity(){
                    console.log("updateValidity",model.$viewValue,scope.$eval(attrs.nxEqualEx));
                    // Only compare values if the second ctrl has a value.
                    if (model.$viewValue !== undefined && model.$viewValue !== '') {
                        model.$setValidity('nxEqualEx', scope.$eval(attrs.nxEqualEx) === model.$viewValue);
                    }
                }
                scope.$watch(attrs.nxEqualEx, updateValidity);
                scope.$watch("$viewValue", updateValidity);

                model.$parsers.push(function (value) {
                    // Mute the nxEqual error if the second ctrl is empty.
                    if (value === undefined || value === '') {
                        model.$setValidity('nxEqualEx', true);
                        return value;
                    }
                    var isValid = value === scope.$eval(attrs.nxEqualEx);
                    model.$setValidity('nxEqualEx', isValid);

                    //We pretend, that parser is succeed always - in order to fix #4419
                    return value; // isValid ? value : undefined;
                });
            }
        };
    }]);
