'use strict';

angular.module('cloudApp')
    .factory('dialogs', function ($http, $uibModal, $q, $location) {
        function openDialog(title, template, url, content, hasFooter, cancellable, params, closable){

            //scope.inline = typeof($location.search().inline) != 'undefined';

            function isInline(){
                return typeof($location.search().inline) != 'undefined';
            }

            // Check 401 against offline
            var modalInstance = $uibModal.open({
                controller: 'DialogCtrl',
                templateUrl: 'views/components/dialog.html',
                animation: !isInline(),
                keyboard:false,
                backdrop:cancellable?true:'static',
                resolve: {
                    settings:function(){
                        return {
                            title:title,
                            template: template,
                            hasFooter: hasFooter,
                            content:content,
                            cancellable: cancellable,
                            params: params,
                            closable: closable || cancellable
                        };
                    },
                    params:function(){
                        return params;
                    }
                }
            });

            /*

            // We are not going to support changing urls on opening dialog: too much trouble
            // Later we should investigate way to user window.history.replaceState to avoid changing browser history on opening dialog.
            // Otherwise it is a mess.
            // Nevertheless we support urls to call dialogs for direct links
            // In code we can call dialog both ways: using link and using this service


            function escapeRegExp(str) {
                return str.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g, "\\$&");
            }
            function clearPath(){
                return $location.$$path.replace(new RegExp("/" + escapeRegExp(url) + '$'),'');
            }

            if(url) {
                $location.path(clearPath() + "/" + url, false);

                modalInstance.result.finally(function () {
                    $location.path(clearPath(), false);
                });
            }
            */



            return modalInstance;
        }

        return {
            alert:function(message, title){
                return openDialog(title, null, null, message, true, true).result;
            },
            confirm:function(message, title){
                return openDialog(title, null, null, message, true, false).result;
            },
            login:function(force){
                return openDialog('Login to Nx Cloud', 'views/login.html', 'login', null, false, !force, null, true).result;
            },
            share:function(systemId, isOwner, share){

                var url = 'share';
                if(share){
                    url += '/' + share.accountEmail;
                }
                return openDialog('Share', 'views/share.html', url, null, false, true,{
                    systemId: systemId,
                    share: share,
                    isOwner: isOwner
                }).result;
            }
        };
    }).controller("DialogCtrl",function($scope, $uibModalInstance,settings){
        $scope.settings = settings;

        $scope.close = function(){
            $uibModalInstance.dismiss('close');
        };

        $scope.ok = function(){
            $uibModalInstance.close('ok');
        };

        $scope.cancel = function(){
            $uibModalInstance.dismiss('cancel');
        };

        $scope.$on('$routeChangeStart', function(){
            $uibModalInstance.close();
        });
    });
