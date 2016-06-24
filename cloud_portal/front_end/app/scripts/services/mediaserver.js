'use strict';

angular.module('cloudApp')
    .factory('mediaserver', ['$http', '$q', 'uuid2', function ($http, $q, uuid2) {

        function gateway(serverId){
             return Config.apiBase + '/systems/' + serverId  + '/proxy';
        }
        var service = {
            getUsers: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getUsers');
            },
            getUserGroups: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getUserGroups');
            },
            getAccessRight: function(systemId){
                return $http.get(gateway(systemId) + '/ec2/getAccessRights');
            },
            saveUser: function(systemId,user){
                return $http.post(gateway(systemId) + '/ec2/saveUser',user);
            },
            userObject: function(fullName, accountEmail){
                return {

                    'email': accountEmail,
                    'name': accountEmail,
                    'fullName': fullName,

                    'isCloud': true,
                    'isEnabled': true,

                    'groupId': '{00000000-0000-0000-0000-000000000000}',
                    'permissions': '',


                    //TODO: Remove the trash below after #VMS-2968
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