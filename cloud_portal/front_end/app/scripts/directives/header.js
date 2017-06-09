'use strict';

angular.module('cloudApp')
    .directive('header',['dialogs', 'cloudApi', 'account', '$location',function (dialogs, cloudApi, account, $location) {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/header.html',
            link:function(scope/*,element,attrs*/){
                scope.inline = typeof($location.search().inline) != 'undefined';

                if(scope.inline){
                    $('body').addClass('inline-portal');
                }

                scope.isActive = function(val){
                    var currentPath = $location.path();
                    if(currentPath.indexOf(val)<0){ // no match
                        return false;
                    }
                    var clearedPath = currentPath.replace(val,'');

                    return clearedPath.split('?')[0].replace('/','') === '';
                };
                scope.login = function(){
                    dialogs.login();
                };
                scope.logout = function(){
                    account.logout();
                };

                account.get().then(function(account){
                    scope.account = account;
                    $('body').removeClass('loading');
                    $('body').addClass('authorized');
                },function(){
                    $('body').removeClass('loading');
                    $('body').addClass('anonymous');
                });
            }
        };
    }]);
