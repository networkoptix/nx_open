'use strict';

angular.module('cloudApp').directive('validateEmail', function() {
    var EMAIL_REGEXP = new RegExp(Config.emailRegex);
    var BRACKETS_REGEXP = /<([^<]+?)>/;

    return {
        require: '?ngModel',
        link: function(scope, elm, attrs, ctrl) {
            // only apply the validator if ngModel is present and Angular has added the email validator
            if (ctrl && ctrl.$validators.email) {
                // this will overwrite the default Angular email validator
                ctrl.$validators.email = function(modelValue) {
                    return ctrl.$isEmpty(modelValue) || EMAIL_REGEXP.test(modelValue);
                };

                ctrl.$parsers.push(function(viewValue) {
                    var extractedEmail = viewValue;
                    if(extractedEmail){
                        var match = extractedEmail.match(BRACKETS_REGEXP);
                        if(match){
                            extractedEmail = match[1];
                        }
                    }
                    return extractedEmail;
                });
            }
        }
    };
});