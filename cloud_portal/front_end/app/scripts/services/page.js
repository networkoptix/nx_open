'use strict';


angular.module('cloudApp')
    .factory('page', ['$rootScope', '$timeout', function ($rootScope, $timeout) {
        return {
            title:function(title, clean){
                $timeout(function(){
                    if(title){
                        if(clean || title.indexOf(L.productName)>=0){
                            $rootScope.pageTitle = title;
                        }else{
                            $rootScope.pageTitle = L.pageTitles.template.replace('{{title}}',title);
                        }
                    }else{
                        $rootScope.pageTitle = L.pageTitles.default;
                    }
                });
            }
        }
    }]);
