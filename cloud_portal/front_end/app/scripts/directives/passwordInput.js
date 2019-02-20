(function () {

    'use strict';

//http://stackoverflow.com/questions/28045427/angularjs-custom-directive-with-input-element-pass-validator-from-outside
    angular
        .module('cloudApp')
        .directive('passwordInput', ['cloudApi', 'languageService', 'nxConfigService',
            function (cloudApi, languageService, nxConfigService) {

                var CONFIG = nxConfigService.getConfig();

                function loadCommonPasswords () {
                    if (!CONFIG.commonPasswordsList) {
                        cloudApi.getCommonPasswords()
                            .then(function (data) {
                                CONFIG.commonPasswordsList = data.data;
                            });
                    }
                }

                function checkComplexity (value) {
                    var classes = [
                        '[0-9]+',
                        '[a-z]+',
                        '[A-Z]+',
                        '[\\W_]+'
                    ];

                    var classesCount = 0;

                    for (var i = 0; i < classes.length; i++) {
                        var classRegex = classes[i];
                        if (new RegExp(classRegex).test(value)) {
                            classesCount++;
                        }
                    }

                    return classesCount;
                }

                return {
                    restrict   : 'E',
                    templateUrl: CONFIG.viewsDir + 'components/password-input.html',
                    require    : 'ngModel',
                    scope      : {
                        ngModel: '=',
                        name   : '=',
                        id     : '=',
                        complex: '=',
                        label  : '=',
                        form   : '='
                    },
                    link       : function (scope, elm) {
                        scope.fairPassword = true;
                        scope.lang = languageService.lang;
                        scope.config = CONFIG;

                loadCommonPasswords(); // Load most common passwords

                scope.untouch = function () {
                    var name = elm[0].id.replace(/'/g, '');
                    scope.form[name].$setUntouched();
                };

                scope.$watch('ngModel', function(value){
                    scope.fairPassword = checkComplexity(value) < CONFIG.passwordRequirements.strongClassesCount;
                });
            }
        };
    }]);
})();
