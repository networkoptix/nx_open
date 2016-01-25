'use strict';

angular.module('cloudApp')
    .controller('StartPageCtrl', function ($scope,cloudApi,$location,$routeParams,dialogs) {
        cloudApi.account().then(function(account){
            if(account){
                $location.path('/systems');
            }
        });

        if($routeParams.callLogin){
            dialogs.login();
        }
    });