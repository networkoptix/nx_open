'use strict';
angular.module('cloudApp')
    .directive('openClientButton', ['cloudApi','process','urlProtocol','dialogs',
    function (cloudApi, process, urlProtocol, dialogs) {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/open-client-button.html',
            scope:{
                system:'='
            },
            link:function(scope, element, attrs, ngModel){
                scope.L = L;
                scope.Config = Config;
                scope.openClient = process.init(function () {
                    return urlProtocol.open(scope.system && scope.system.id || "").catch(function(error){
                        dialogs.noClientDetected();
                        return true;
                    });
                });
            }
        };
    }]);