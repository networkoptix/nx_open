'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $resource) {
        return {
            ping: $resource('/ec2/ping'),
            settings: $resource('/ec2/settings'),
            restart: function() { return $http.post('/ec2/restartServer'); }
        };
    });
