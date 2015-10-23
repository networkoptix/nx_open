'use strict';

angular.module('cloudApp')
    .controller('StartPageCtrl', function ($scope,cloudApi,$location) {
        cloudApi.account().then(function(account){
            if(account){
                $location.path('/systems');
            }
        });
    });