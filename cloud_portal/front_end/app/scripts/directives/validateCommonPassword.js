(function () {

    'use strict';

    angular
        .module('cloudApp')
        .directive('validateCommonPassword', ValidateCommonPassword);

    ValidateCommonPassword.$inject = [ 'configService' ];

    function ValidateCommonPassword(configService) {

        return {
            require: 'ngModel',
            link   : function (scope, elm, attrs, ctrl) {
                ctrl.$validators.common = function (modelValue) {
                    if (!modelValue || !configService.config.commonPasswordsList) {
                        return;
                    }

                    // Check if password is directly in common list
                    var commonPassword = configService.config.commonPasswordsList[ modelValue ];

                    if (!commonPassword) {
                        // Check if password is in uppercase and it's lowercase value is in common list
                        commonPassword = modelValue.toUpperCase() === modelValue &&
                            configService.config.commonPasswordsList[ modelValue.toLowerCase() ];
                    }

                    return !commonPassword;
                };


            }
        };
    }
})();
