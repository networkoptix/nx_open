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
    }]).directive('enterButton', ['$timeout', function($timeout) {
        return {
            restrict: 'A',
            link : function(scope, $element) {
                var $body = $('body');

                function keyHandler(event){
                    if (event.keyCode === 13) {
                        if ($element.is(':disabled')) {
                            return false;
                        }
                        $element.click();
                        event.preventDefault();
                        event.stopPropagation();
                    }
                }
    
                $timeout(function () {
                    $body.bind( 'keyup.enter', keyHandler );
                },100);
                scope.$on('$destroy',function(){
                    $body.unbind('keyup.enter');
                });
            }
        };
    }]);
