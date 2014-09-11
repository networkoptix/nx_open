/**
 * Created by noptix on 09/09/14.
 */
'use strict';

angular.module('webadminApp')
    .directive('icon', function () {
        return {
            restrict: 'E',
            link: function (scope, element, attrs) {
                attrs.onTrue = attrs.onTrue || 'ok';
                var isTrue = attrs.for=="true";
                var icon = isTrue ? attrs.onTrue : attrs.onFalse;
                attrs.title = isTrue? attrs.titleTrue : attrs.titleFalse;

                var template = icon ? '<span class="glyphicon glyphicon-' + icon + ' ' + attrs.class + '" title="' + attrs.title + '"></span>' : '';
                $(element).replaceWith(template);
            }
        };
    });
