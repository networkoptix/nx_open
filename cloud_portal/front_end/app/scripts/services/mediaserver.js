'use strict';

angular.module('cloudApp')
    .factory('mediaserver', ['$http', '$q', 'uuid2', '$localStorage','$base64', function ($http, $q, uuid2, $localStorage, $base64) {

        function gateway(serverId){
            return Config.apiBase + '/systems/' + serverId  + '/proxy';
        }
        function direct_gateway(serverId){
            return Config.gatewayUrl + '/' + serverId;
        }

        function emulateResponse(data){
            var deferred = $q.defer();
            deferred.resolve({data:data});
            return deferred.promise;
        }
        var service = function(systemId){ 
            return{
                login:function(auth){
                    return $localStorage[systemId] = {auth: auth};
                },
                logout:function(){
                    $localStorage[systemId] = null;
                },
                auth:function(){
                    if($localStorage[systemId] && $localStorage[systemId].auth){
                        return $localStorage[systemId].auth;
                    }
                    return null;
                },
                
                _get:function(url){
                    var auth = this.auth();
                    if(auth){
                        if(url.indexOf('?') == -1){
                            url += '?auth=';
                        }else{
                            url += '&auth=';
                        }
                        return $http.get(direct_gateway(systemId) + url + auth);
                    }
                    return $http.get(gateway(systemId) + url);
                },
                _post:function(url, data){
                    return $http.post(gateway(systemId) + url, data);
                },
                
                getCurrentUser: function(){
                    return this._get('/ec2/getCurrentUser');
                },
                getAggregatedUsersData: function(){
                    return this._get('/api/aggregator?exec_cmd=ec2%2FgetUsers&exec_cmd=ec2%2FgetPredefinedRoles&exec_cmd=ec2%2FgetUserGroups');
                },
                saveUser: function(user){
                    return this._post('/ec2/saveUser', this.cleanUserObject(user));
                },
                deleteUser: function(userId){
                    return this._post('/ec2/removeUser', {id:userId});
                },
                cleanUserObject:function(user){ // Remove unnesesary fields from the object
                    var supportedFields = ['email', 'userId', 'groupId', 'permissions', 'isCloud', 'isEnabled'];
                    var cleanedUser = {};
                    for(var i in supportedFields){
                        cleanedUser[supportedFields[i]] = user[supportedFields[i]];
                    }
                    return cleanedUser;
                },
                userObject: function(fullName, email){
                    return {
                        'canBeEdited' : true,
                        'canBeDeleted' : true,

                        'email': email,

                        'isCloud': true,
                        'isEnabled': true,
    
                        'groupId': '{00000000-0000-0000-0000-000000000000}',
                        'permissions': '',

                        //TODO: Remove the trash below after #VMS-2968
                        'name': email,
                        'fullName': fullName,
                    }
                }
            }   
        };
        return service;
    }]);