'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $resource) {
        return {
            getSettings: function() { return $http.get('/api/moduleInformation'); },
            saveSettings: function(systemName,port) { return $http.post('/api/configure?systemName=' + systemName + '&port=' + port); },
            changePassword: function(password,oldPassword) { return $http.post('/api/configure?password=' + password  + '&oldPassword=' + oldPassword); },
            joinSystem: function(url,password){return $http.post('/api/joinSystem?password=' + password  + '&url=' + encodeURIComponent(url)); },
            pingSystem: function(url,password){return $http.post('/api/pingSystem?password=' + password  + '&url=' + encodeURIComponent(url)); },
            restart: function() { return $http.post('/api/restart'); },
            getStorages: function(){ return $http.get('/api/storageSpace'); },

            getMediaServer: function(id){return $http.get('/ec2/getMediaServers?id=' + id); },
            saveMediaServer: function(info){return $http.post('/ec2/saveMediaServer',info); }
        };
    });
