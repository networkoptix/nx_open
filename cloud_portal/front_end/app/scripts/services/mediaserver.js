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
                digest:function(login, password, realm, nonce){
                    var digest = md5(login + ':' + realm + ':' + password);
                    var method = md5('GET:');
                    var authDigest = md5(digest + ':' + nonce + ':' + method);
                    var auth = $base64.encode(login + ':' + nonce + ':' + authDigest);
                    /*console.log("digest calc", {
                        login:login,
                        password:password,
                        realm:realm,
                        nonce:nonce,
                        digest:digest,
                        method:method,
                        authDigest:authDigest,
                        auth:auth
                    });*/
                    return auth;
                },
                login:function(nonce){
                    return $localStorage[systemId] = {auth: this.digest($localStorage.email, $localStorage.password, Config.realm, nonce)};
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
                    if(auth && Config.enableUrlAuth){
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
                changeSystemName:function(systemName){
                    return this._post('/web/api/configure', {
                        systemName:systemName
                    });
                },
                cleanUserObject:function(user){ // Remove unnesesary fields from the object
                    return user; //TODO: uncomment after #VMS-2968
    
                    var supportedFields = ['email', 'userId', 'groupId', 'permissions', 'isCloud'];
                    var cleanedUser = {};
                    for(var i in supportedFields){
                        cleanedUser[supportedFields[i]] = user[supportedFields[i]];
                    }
                    return cleanedUser;
                },
                userObject: function(fullName, accountEmail){
                    return {
                        'canBeEdited' : true,
                        'canBeDeleted' : true,
    
                        'email': accountEmail,
                        'name': accountEmail,
    
                        'isCloud': true,
                        'isEnabled': true,
    
                        'groupId': '{00000000-0000-0000-0000-000000000000}',
                        'permissions': '',
    
    
                        //TODO: Remove the trash below after #VMS-2968
                        'accountEmail': accountEmail,
                        'fullName': fullName,
                        'id': '{' + uuid2.newuuid() + '}',
                        'isAdmin': false,
                        'cryptSha512Hash': '',
                        'digest': 'invalid_digest',
                        'hash': 'invalid_hash',
                        'isLdap': false,
                        'parentId': '{00000000-0000-0000-0000-000000000000}',
                        'realm': 'VMS',
                        'typeId': '{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}',
                        'url': ''
                    }
                }
            }   
        };
        return service;
    }]);