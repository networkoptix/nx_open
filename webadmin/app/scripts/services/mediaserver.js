'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $resource) {
        return {
            ping: $resource('/ec2/ping'),
            getSettings: function() { return $http.get('/api/moduleInformation'); },
            saveSystemName: function(systemName) { return $http.post('/api/configure?systemName=' + systemName); },
            savePort: function(port) { return $http.post('/api/configure?port=' + port); },
            savePassword: function(password) { return $http.post('/api/configure?systemName=' + password); },
            restart: function() { return $http.post('/api/restart'); },
            getStorages: function(){ return $http.get('/api/storageSpace');}
        };
    });
