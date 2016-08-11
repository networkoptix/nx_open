'use strict';

angular.module('cloudApp')
    .factory('mediaserver', ['$http', '$q', 'uuid2', function ($http, $q, uuid2) {

        function gateway(serverId){
            return Config.apiBase + '/systems/' + serverId  + '/proxy';
        }

        function emulateResponse(data){
            var deferred = $q.defer();
            deferred.resolve({data:data});
            return deferred.promise;
        }
        var service = {
            getCurrentUser: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getCurrentUser');
            },
            getAggregatedUsersData: function(systemId){
                return $http.get(gateway(systemId) + '/api/aggregator?exec_cmd=ec2%2FgetUsers&exec_cmd=ec2%2FgetPredefinedRoles&exec_cmd=ec2%2FgetUserGroups');
            },
            getUsers: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getUsers');
            },
            getPredefinedRoles: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getPredefinedRoles');
            },
            getUserGroups: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getUserGroups');
            },
            saveUser: function(systemId, user){
                return $http.post(gateway(systemId) + '/ec2/saveUser', this.cleanUserObject(user));
            },
            deleteUser: function(systemId, userId){
                return $http.post(gateway(systemId) + '/ec2/removeUser', {id:userId});
            },
            changeSystemName:function(systemId,systemName){
                return $http.post(gateway(systemId) + '/web/api/configure', {
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
        };
        return service;
    }]);