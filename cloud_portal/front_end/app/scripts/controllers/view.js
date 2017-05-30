'use strict';

angular.module('cloudApp')
    .controller('ViewPageCtrl', ['$scope', 'account',
    function ($scope, account) {
        account.requireLogin();

        $("body").addClass("webclient-page");
        $scope.$on('$destroy', function( event ) {
            $("body").removeClass("webclient-page");
        });
    }]);