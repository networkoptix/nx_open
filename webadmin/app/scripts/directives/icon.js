/**
 * Created by noptix on 09/09/14.
 */
'use strict';

angular.module('webadminApp')
    .directive('icon', function () {
        return {
            restrict: 'E',
            scope:{
                for:"="
            },
            template:'<span ng-show="for || onFalse" class="glyphicon glyphicon-{{for?onTrue:onFalse}} {{class}}" title="{{for?titleTrue:titleFalse}}"></span>',
            link: function (scope, element, attrs) {
                scope.onTrue = attrs.onTrue || 'ok';
                scope.onFalse = attrs.onFalse;
                scope.class = attrs.class;
                scope.titleTrue = attrs.titleTrue;
                scope.titleFalse = attrs.titleFalse;
/*

                var isTrue = scope.for=="true";
                var icon = isTrue ? attrs.onTrue : attrs.onFalse;
                attrs.title = isTrue? attrs.titleTrue : attrs.titleFalse;

                var template = icon ? '<span class="glyphicon glyphicon-' + icon + ' ' + attrs.class + '" title="' + attrs.title + '"></span>' : '';
                //$(element).append(template);

                /*scope.$watch("for",function(){
                   // $(element).children().remove();
                    var isTrue = scope.for=="true";
                    var icon = isTrue ? attrs.onTrue : attrs.onFalse;
                    attrs.title = isTrue? attrs.titleTrue : attrs.titleFalse;

                    var template = icon ? '<span class="glyphicon glyphicon-' + icon + ' ' + attrs.class + '" title="' + attrs.title + '"></span>' : '';
                    //$(element).append(template);
                })*/
            }
        };
    });
