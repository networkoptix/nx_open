'use strict';

angular.module('webadminApp')
    .factory('mediaserver', function ($http, $resource) {

        var cacheModuleInfo = null;

        return {
            getSettings: function(url) {
                url = url || "";

                if(url==""){// Кешируем данные о сервере, чтобы не запрашивать 10 раз
                    if(cacheModuleInfo == null){
                        cacheModuleInfo = $http.get(url + '/api/moduleInformation')
                    }
                    return cacheModuleInfo;
                }
                console.log("get settings",url + '/api/moduleInformation');
                return $http.get(url + '/api/moduleInformation',{
                    timeout: 3*1000
                });
            },
            saveSettings: function(systemName,port) { return $http.post('/api/configure?systemName=' + systemName + '&port=' + port); },
            changePassword: function(password,oldPassword) { return $http.post('/api/configure?password=' + password  + '&oldPassword=' + oldPassword); },
            mergeSystems: function(url,password,keepMySystem){return $http.post('/api/mergeSystems?password=' + password  + '&url=' + encodeURIComponent(url) + "&takeRemoteSettings=" + (!keepMySystem)); },
            pingSystem: function(url,password){return $http.post('/api/pingSystem?password=' + password  + '&url=' + encodeURIComponent(url)); },
            restart: function() { return $http.post('/api/restart'); },
            getStorages: function(){ return $http.get('/api/storageSpace'); },
            saveStorages:function(info){return $http.post('/ec2/saveStorages',info); },
            discoveredPeers:function(){return $http.get('/api/discoveredPeers'); },
            getMediaServer: function(id){return $http.get('/ec2/getMediaServersEx?id=' + id); },
            getMediaServers: function(){return $http.get('/ec2/getMediaServersEx'); },
            saveMediaServer: function(info){return $http.post('/ec2/saveMediaServer',info); },
            statistics:function(url){
                url = url || "";
                return $http.get(url + '/api/statistics');
            },
            getCurrentUser:function(){return $http.post('/api/getCurrentUser');}
        };
    });
