'use strict';

angular.module('cloudApp')
    .controller('StartPageCtrl', ['$scope', 'cloudApi', '$location', '$routeParams', 'dialogs',
        'accountService',

        function ($scope, cloudApi, $location, $routeParams, dialogs, accountService) {
            accountService.redirectAuthorised();

            $scope.userEmail = accountService.getEmail();

            if ($routeParams.callLogin) {
                dialogs.login();
            }
        }]);
