'use strict';

angular.module('cloudApp')
    .controller('StartPageCtrl', function ($scope,cloudApi,$location,$routeParams,dialogs,account) {
        account.redirectAuthorised();

        if($routeParams.callLogin){
            dialogs.login();
        }
    });