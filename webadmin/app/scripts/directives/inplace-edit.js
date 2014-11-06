/**
 * Created by noptix on 10/09/14.
 */

angular.module('webadminApp')
    .directive('inplaceEdit', function () {
        return {
            restrict: 'E',
            templateUrl:'views/inplace-edit.html',
            scope: {
                model:"=ngModel"
            },
            link:function(scope,element,attrs){
                var oldvalue = scope.model;

                scope.type = attrs.type || "text";
                scope.placeholder = attrs.placeholder;
                scope.inputId=attrs.inputId;

                scope.edit = function(){
                    oldvalue = scope.model;
                    element.find("input").val(oldvalue);

                    element.find(".read-container").hide();
                    element.find(".edit-container").show();
                    element.find("input").focus();
                };
                scope.save = function(){
                    if(oldvalue != scope.model){

                        // Save!
                        scope.$parent.$eval(attrs.nxOnsave);
                    }

                    oldvalue = scope.model;
                    element.find(".read-container").show();
                    element.find(".edit-container").hide();
                };
                scope.cancel = function(){
                    scope.model = oldvalue;
                    scope.$apply();
                    element.find(".read-container").show();
                    element.find(".edit-container").hide();
                };

                element.find(".edit-container").hide();
            }
        };
    });
