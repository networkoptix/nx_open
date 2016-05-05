'use strict';


angular.module('webadminApp').directive('disableAutocomplete',function(){
    return {
        restrict: 'A',
        link :  function (scope, element/*, attrs*/) {
            element.attr('autocomplete', 'off');
            element.prepend('<input style="display:none" type="text" >' +
                '<input style="display:none" type="password">');

        }
    };
});
