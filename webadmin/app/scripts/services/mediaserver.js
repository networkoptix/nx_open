'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $resource) {
        return {
            ping: $resource('/ec2/ping'),
            getSettings: function() { return $http.get('/api/moduleInformation'); },
            saveSettings: function(systemName) { return $http.post('/api/configure?systemName=' + systemName); },
            restart: function() { return $http.post('/api/restart'); }
        };
    });
