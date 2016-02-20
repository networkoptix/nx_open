'use strict';

angular.module('webadminApp')
    .directive('autofocus', ['$timeout', function($timeout) {
        return {
            restrict: 'A',
            scope:{
                autofocus: '='
            },
            link : function(scope, $element) {
                function update() {
                    if(typeof(scope.autofocus) === 'undefined' || scope.autofocus) {
                        $timeout(function () {
                            $element[0].focus();
                        });
                    }
                }

                scope.$watch('autofocus',update);
                update();
            }
        }
    }]).directive('enterButton', function() {
        return {
            restrict: 'A',
            link : function(scope, $element) {
                var $body = $('body');

                function keyHandler(event){
                    if(event.keyCode == 13){
                        if($element.is(':disabled')){
                            return false;
                        }
                        $element.click();
                    }
                    return false;
                }

                $body.bind( 'keyup.enter', keyHandler );
                scope.$on('$destroy',function(){
                    $body.unbind('keyup.enter');
                });
            }
        }
    });