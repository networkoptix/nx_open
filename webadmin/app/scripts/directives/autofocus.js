'use strict';

angular.module('webadminApp')
    .directive('autoFocus', ['$timeout', function($timeout) {
        return {
            restrict: 'A',
            link : function(scope, $element, attr) {
                if(typeof(attr.autoFocus) === 'undefined' ||
                    attr.autoFocus === '' ||
                    attr.autoFocus === 'true' ||
                    attr.autoFocus === true) {
                    $timeout(function () {
                        $element[0].focus();
                    });
                }
            }
        };
    }]).directive('enterButton', function() {
        return {
            restrict: 'A',
            link : function(scope, $element) {
                var $body = $('body');

                function keyHandler(event){
                    return false;
                }

                $body.bind( 'keyup.enter', keyHandler );
                scope.$on('$destroy',function(){
                    $body.unbind('keyup.enter');
                });
            }
        };
    });
