'use strict';


angular.module('cloudApp').run(function($http,$templateCache) {
        $http.get('views/login.html', {cache: $templateCache});
        $http.get('views/share.html', {cache: $templateCache});
    })
    .factory('dialogs', function ($http, $uibModal, $q, $location) {
        function openDialog(settings ){

            function isInline(){
                return typeof($location.search().inline) != 'undefined';
            }

            var modalInstance = null;
            settings.params = settings.params || {};
            settings.params.getModalInstance = function(){return modalInstance;};
            // Check 401 against offline
            modalInstance = $uibModal.open({
                size: settings.size || 'sm',
                controller: 'DialogCtrl',
                templateUrl: 'views/components/dialog.html',
                animation: !isInline(),
                keyboard:false,
                backdrop:settings.cancellable?true:'static',
                resolve: {
                    settings:function(){
                        return {
                            title:settings.title,
                            template: settings.template,
                            hasFooter: settings.hasFooter,
                            content:settings.content,
                            cancellable: settings.cancellable,
                            params: settings.params,
                            actionLabel: settings.actionLabel || L.dialogs.okButton,
                            closable: settings.closable || settings.cancellable,
                            buttonClass: settings.buttonType? 'btn-'+ settings.buttonType : 'btn-primary'
                        };
                    },
                    params:function(){
                        return settings.params;
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
                return openDialog({
                    title:title,
                    content: message,
                    hasFooter: true,
                    cancellable:true,
                    closable:true}).result;
            },
            confirm:function(message, title, actionLabel, actionType){
                //title, template, url, content, hasFooter, cancellable, params, closable, actionLabel, buttonType, size
                return openDialog({
                    title: title,
                    content:message,
                    hasFooter: true,
                    cancellable:false,
                    closable: false,
                    actionLabel:actionLabel,
                    buttonType:actionType
                }).result;
            },
            login:function(force){
                return openDialog({
                    title: L.dialogs.loginTitle,
                    template: 'views/login.html',
                    url: 'login',
                    hasFooter: false,
                    cancellable: !force,
                    closable:true}).result;
            },
            share:function(systemId, isOwner, share){
                var url = 'share';
                var title = L.sharing.shareTitle;
                if(share){
                    url += '/' + share.accountEmail;
                    title = L.sharing.editShareTitle;
                }
                return openDialog({
                    title:title,
                    template: 'views/share.html',
                    url: url,
                    hasFooter: false,
                    cancellable:true,
                    params: {
                        systemId: systemId,
                        share: share,
                        isOwner: isOwner
                    }
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
