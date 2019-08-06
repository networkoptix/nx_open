(function () {

    'use strict';

    angular
        .module('cloudApp')
        .directive('validateWeakPassword', ValidateWeakPassword);

    ValidateWeakPassword.$inject = [ 'nxConfigService' ];

    function ValidateWeakPassword(nxConfigService) {

        var CONFIG = nxConfigService.getConfig();

        function checkComplexity(value) {
            var classes = [
                '[0-9]+',
                '[a-z]+',
                '[A-Z]+',
                '[\\W_]+'
            ];

            var classesCount = 0;

            for (var i = 0; i < classes.length; i++) {
                var classRegex = classes[ i ];
                if (new RegExp(classRegex).test(value)) {
                    classesCount++;
                }
            }

            return classesCount;

        }

        return {
            require: 'ngModel',
            link   : function (scope, elm, attrs, ctrl) {
                ctrl.$validators.weak = function (modelValue) {
                    if (!modelValue) {
                        return;
                    }

                    return checkComplexity(modelValue) >= CONFIG.passwordRequirements.minClassesCount;
                };


            }
        };
    }
})();
