(function () {

    'use strict';

    angular
        .module('cloudApp')
        .directive('validateCommonPassword', ValidateCommonPassword);

    ValidateCommonPassword.$inject = [ 'nxConfigService' ];

    function ValidateCommonPassword(nxConfigService) {

        var CONFIG = nxConfigService.getConfig();

        return {
            require: 'ngModel',
            link   : function (scope, elm, attrs, ctrl) {
                ctrl.$validators.common = function (modelValue) {
                    if (!modelValue || !CONFIG.commonPasswordsList) {
                        return;
                    }

                    // Check if password is directly in common list
                    var commonPassword = CONFIG.commonPasswordsList[ modelValue ];

                    if (!commonPassword) {
                        // Check if password is in uppercase and it's lowercase value is in common list
                        commonPassword = modelValue.toUpperCase() === modelValue &&
                            CONFIG.commonPasswordsList[ modelValue.toLowerCase() ];
                    }

                    return !commonPassword;
                };


            }
        };
    }
})();
