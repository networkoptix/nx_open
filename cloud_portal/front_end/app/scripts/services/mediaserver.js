'use strict';

angular.module('cloudApp')
    .factory('mediaserver', ['$http', '$q', 'uuid2', function ($http, $q, uuid2) {

        function gateway(serverId){
             return Config.apiBase + '/systems/' + serverId  + '/proxy';
        }
        var service = {
            getCurrentUser: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getCurrentUser');
            },
            getUsers: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getUsers');
            },
            getUserGroups: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getUserGroups');
            },
            getAccessRight: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getAccessRights');
            },
            saveUser: function(systemId, user){
                return $http.post(gateway(systemId) + '/ec2/saveUser', this.cleanUserObject(user));
            },
            deleteUser: function(systemId, userId){
                return $http.post(gateway(systemId) + '/ec2/removeUser', {id:userId});
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
                    'email': accountEmail,
                    'name': accountEmail,

                    'isCloud': true,
                    'isEnabled': true,

                    'groupId': '{00000000-0000-0000-0000-000000000000}',
                    'permissions': '',


                    //TODO: Remove the trash below after #VMS-2968
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