'use strict';

angular.module('cloudApp')
    .factory('cloudApi', function ($http, $q) {

        var apiBase = Config.apiBase;
        var accountCache = null;
        return {
            account:function(){
                var defer = $q.defer();
                if(!accountCache){
                    $http.get(apiBase + '/account').then(function(response){
                        accountCache = response.data;
                        defer.resolve(accountCache);
                    },function(error){
                        accountCache = null;
                        defer.resolve(null);
                        return null
                    })
                }else{
                    defer.resolve(accountCache);
                }
                return defer.promise;
            },

            login:function(email, password, remember){
                return $http.post(apiBase + '/account/login',{
                    email: email,
                    password: password,
                    remember: remember
                });
            },
            logout:function(){
                return $http.post(apiBase + '/account/logout');
            },
            register:function(email,password,firstName,lastName,subscribe){
                return $http.post(apiBase + '/account/register',{
                    email:email,
                    password:password,
                    first_name:firstName,
                    last_name:lastName,
                    subscribe:subscribe
                });
            },

            notification_send:function(userEmail,type,message){
                return $http.post(apiBase.replace("/api","/notifications") + '/send',{
                    user_email:userEmail,
                    type:type,
                    message:message
                });
            },

            accountPost:function(firstName,lastName,subscribe){
                return $http.post(apiBase + '/account',{
                    first_name:firstName,
                    last_name:lastName,
                    subscribe:subscribe
                });
            },
            changePassword:function(newPassword,oldPassword){
                return $http.post(apiBase + '/account/changePassword',{
                    new_password:newPassword,
                    old_password:oldPassword
                });
            },
            restorePasswordRequest:function(userEmail){
                return $http.post(apiBase + '/account/restorePassword',{
                    user_email:userEmail
                });
            },
            restorePassword:function(code, newPassword){
                return $http.post(apiBase + '/account/restorePassword',{
                    code:code,
                    new_password:newPassword
                });
            },
            reactivate:function(userEmail){
                return $http.post(apiBase + '/account/activate',{
                    user_email:userEmail
                });
            },
            activate:function(code){
                return $http.post(apiBase + '/account/activate',{
                    code:code
                });
            },
            systems:function(systemId){
                return $http.get(apiBase + '/systems' + (systemId?('/' + systemId):''));
            },
            getCommonPasswords:function(){
                return $http.get('/scripts/commonPasswordsList.json');
            },
            users:function(systemId){
                return $http.get(apiBase + '/systems/' + systemId + '/users');
            },
            share:function(systemId, userEmail, role){
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role:role
                });
            },
            unshare:function(systemId,userEmail){
                return $http.post(apiBase + '/systems/' + systemId + '/users', {
                    user_email: userEmail,
                    role: Config.accessRolesSettings.unshare
                });
            },
            accessRoles: function(){
                // TODO: cache this request
                return $http.get(apiBase + '/systems/accessRoles');
            }
        }

    });