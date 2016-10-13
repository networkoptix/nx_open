'use strict';
angular.module('cloudApp')
    .directive('openClientButton', ['cloudApi','process','urlProtocol','dialogs',
    function (cloudApi, process, urlProtocol, dialogs) {
        return {
            restrict: 'E',
            templateUrl: 'static/views/components/open-client-button.html',
            scope:{
                system:'='
            },
            link:function(scope, element, attrs, ngModel){
                scope.L = L;
                scope.Config = Config;
                scope.openClient = process.init(function () {
                    return urlProtocol.open(scope.system.id).catch(function(error){
                        dialogs.noClientDetected();
                        return true;
                    });
                });
            }
        };
    }]);