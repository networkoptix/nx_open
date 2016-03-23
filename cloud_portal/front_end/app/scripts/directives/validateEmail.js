'use strict';

angular.module('cloudApp').directive('validateEmail', function() {
    var EMAIL_REGEXP = new RegExp(Config.emailRegex);

    return {
        require: '?ngModel',
        link: function(scope, elm, attrs, ctrl) {
            console.log("validate email");
            // only apply the validator if ngModel is present and Angular has added the email validator
            if (ctrl && ctrl.$validators.email) {

                // this will overwrite the default Angular email validator
                ctrl.$validators.email = function(modelValue) {

                    console.log("validate email", modelValue);

                    return ctrl.$isEmpty(modelValue) || EMAIL_REGEXP.test(modelValue);
                };
            }
        }
    };
});