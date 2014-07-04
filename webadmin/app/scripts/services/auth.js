'use strict';

angular.module('webadminApp')
    .service('auth', function ($http) {
        this.login = function (user) {
            return $http.post('/ec2/login', {username: user.username, password: user.password});
        };
    });
