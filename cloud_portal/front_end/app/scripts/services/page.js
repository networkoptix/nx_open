'use strict';


angular.module('cloudApp')
    .factory('page', ['$rootScope', '$timeout', function ($rootScope, $timeout) {
        return {
            title:function(title){
                $timeout(function(){
                    if(title){
                        $rootScope.pageTitle = L.pageTitles.template.replace("{{title}}",title);
                    }else{
                        $rootScope.pageTitle = L.pageTitles.default;
                    }
                });
            }
        }
    }]);
