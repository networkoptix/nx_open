'use strict';

angular.module('cloudApp')
    .factory('account', function (cloudApi, dialogs) {
        return {
            redirectAuthorised:function(){
                cloudApi.account().then(function(account){
                    if(account){
                        $location.path(Config.redirectAuthorised);
                    }
                });
            }
        }
    });