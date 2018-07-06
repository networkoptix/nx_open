'use strict';


angular.module('cloudApp').run(['$http','$templateCache', function($http,$templateCache) {
        $http.get(Config.viewsDir + 'dialogs/login.html', {cache: $templateCache});
        $http.get(Config.viewsDir + 'dialogs/share.html', {cache: $templateCache});
        $http.get(Config.viewsDir + 'components/dialog.html', {cache: $templateCache});
    }])
    .factory('dialogs', ['$http', '$uibModal', '$q', '$location', 'ngToast', function ($http, $uibModal, $q, $location, ngToast) {

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
                templateUrl: Config.viewsDir + 'components/dialog.html',
                animation: !isInline(),
                keyboard:settings.cancellable,
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
                            cancelLabel: settings.cancelLabel || L.dialogs.cancelButton,
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
                return str.replace(/[\-\[\]\/\{\}\(\)\*\+\?\.\\\^\$\|]/g, '\\$&');
            }
            function clearPath(){
                return $location.$$path.replace(new RegExp('/' + escapeRegExp(url) + '$'),'');
            }

            if(url) {
                $location.path(clearPath() + '/' + url, false);

                modalInstance.result.finally(function () {
                    $location.path(clearPath(), false);
                });
            }
            */



            return modalInstance;
        }

        return {
            notify:function(message, type, hold){
                type = type || 'info';

                return ngToast.create({
                    className: type,
                    content:message,
                    dismissOnTimeout: !hold,
                    dismissOnClick: !hold,
                    dismissButton: hold
                });
            },
            dismissNotifications:function(){
                return ngToast.dismiss();
            },
            alert:function(message, title){
                return openDialog({
                    title:title,
                    content: message,
                    hasFooter: true,
                    cancellable:true,
                    closable:true}).result;
            },
            confirm:function(message, title, actionLabel, actionType, cancelLabel){
                //title, template, url, content, hasFooter, cancellable, params, closable, actionLabel, buttonType, size
                return openDialog({
                    title: title,
                    content:message,
                    hasFooter: true,
                    cancellable:false,
                    closable: false,
                    actionLabel:actionLabel,
                    cancelLabel:cancelLabel,
                    buttonType:actionType
                }).result;
            },
            login:function(keepPage){
                return openDialog({
                    title: L.dialogs.loginTitle,
                    template: Config.viewsDir + 'dialogs/login.html',
                    url: 'login',
                    hasFooter: false,
                    cancellable: !keepPage,
                    closable:true,
                    params:{
                        redirect: !keepPage
                    }}).result;
            },
            share:function(system, user){
                var url = 'share';
                var title = L.sharing.shareTitle;
                if(user){
                    title = L.sharing.editShareTitle;
                }
                return openDialog({
                    title:title,
                    template: Config.viewsDir + 'dialogs/share.html',
                    url: url,
                    hasFooter: false,
                    cancellable:true,
                    params: {
                        system: system,
                        user: user,
                        isOwner: system.isMine
                    }
                }).result;
            },

            disconnect:function(systemId){
                var title = L.system.confirmDisconnectTitle;

                return openDialog({
                    title:title,
                    template: Config.viewsDir + 'dialogs/disconnect.html',
                    hasFooter: false,
                    cancellable:true,
                    params: {
                        systemId: systemId
                    }
                }).result;
            },
            rename:function(systemId, systemName){
                var title = L.system.confirmRenameTitle;

                return openDialog({
                    title:title,
                    template: Config.viewsDir + 'dialogs/rename.html',
                    hasFooter: false,
                    cancellable:true,
                    params: {
                        systemId: systemId,
                        systemName: systemName
                    }
                }).result;
            },
            merge:function(system){
                var title = L.system.mergeSystemTitle;

                return openDialog({
                    title: title,
                    template: Config.viewsDir + 'dialogs/merge.html',
                    hasFooter: false,
                    cancellable:true,
                    params:{
                        system: system
                    }
                }).result;
            },
            noClientDetected:function(){
                // message, title, actionLabel, actionType
                return this.confirm(null /*L.downloads.noClientDetectedMessage*/,
                    L.downloads.noClientDetectedTitle, L.downloads.action)
                    .then(function(){
                        $location.path('/download');
                    });
                // return this.notify(L.errorCodes.cantOpenClient, 'danger', true);
            },


            closeMe:function($scope, result){

                // TODO: We must replace this hack with something more angular-way,
                // but I can't figure out yet, how to implement dialog service and pass parameters to controllers
                // we need something like modalInstance
                function findSettings($scope){
                    return $scope.settings || $scope.$parent && findSettings($scope.$parent) || null;
                }

                var dialogSettings = findSettings($scope);

                if(dialogSettings && dialogSettings.params.getModalInstance){
                    if(result){
                        dialogSettings.params.getModalInstance().close(result);
                    }else{
                        dialogSettings.params.getModalInstance().dismiss();
                    }
                }
            },
            getSettings:function($scope){

                // TODO: We must replace this hack with something more angular-way,
                // but I can't figure out yet, how to implement dialog service and pass parameters to controllers
                // we need something like modalInstance

                return $scope.settings || $scope.$parent && this.getSettings($scope.$parent) || null;
            }
        };
    }]).controller('DialogCtrl',['$scope', '$uibModalInstance','settings', function($scope, $uibModalInstance,settings){
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
    }]);
