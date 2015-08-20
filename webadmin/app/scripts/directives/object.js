'use strict';

angular.module('webadminApp')
    .directive('objecter', function () {
        return {
            restrict: 'E',
            scope:{
                ngId:'=',
                ngParam:'=',
                ngSource:'=',
                ngName:'='
            },
            link: function (scope, element, attrs) {
                var $_current = element;


                //scope.$watch('[' + attrs.ngData + ',' + attrs.ngParam + ']', function (newVal) {
                    $_current.html(''); // destroy object

                    var content = '';
                    var paramsplain ='';
                    _.forEach(scope.ngParam, function (value, name) {
                        content += '<param name="' + name + '" value="' + value + '">';
                        paramsplain+=name+'="'+value +'" ';
                    });

                    content +=
                        '<embed  width="100%" height="100%" ' +
                        'id="' + scope.ngName +
                        '" name="' + scope.ngName +
                        '" src="' + scope.ngSource +
                        '" ' + paramsplain +
                        '></embed>';

                    var objectHTML =
                        '<object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" ' +
                        'id="' + scope.ngId +
                        '" data="' + scope.ngSource +
                        '">' +
                        content +
                        '</object>';



                if(Config.allowDebugMode && Config.debug.video){
                    console.log("objecter",objectHTML);
                }
                    $_current.html(objectHTML);

                //}, true);
            }
        }
    });
