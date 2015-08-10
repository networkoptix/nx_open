'use strict';

angular.module('webadminApp')
    .directive('object', function () {
        return {
            restrict: 'E',
            scope:{
                ngParam:'=',
                ngSource:'=',
                ngName:'='
            },
            link: function (scope, element, attrs) {
                var $_current = element;

                //scope.$watch('[' + attrs.ngData + ',' + attrs.ngParam + ']', function (newVal) {
                    var $_clone = element.clone().attr('data', scope.ngSource),
                        content = '';
                    var paramsplain ='';
                    _.forEach(scope.ngParam, function (value, name) {
                        content += '<param name="' + name + '" value="' + value + '">';
                        paramsplain+=name+'="'+value +'" ';
                    });

                    $_current.replaceWith($_clone.html(content +=
                        '<embed  width="100%" height="100%" ' +
                        'id="' + scope.ngName +
                        '" name="' + scope.ngName +
                        '" src="' + scope.ngSource +
                        '" ' + paramsplain +
                        '></embed>'));
                    //$_current = $_clone;
                //}, true);

            }
        }
    });
