'use strict';

angular.module('webadminApp')
    .factory('dialogs', ['$http', '$modal', '$q', '$location', function ($http, $modal, $q, $location) {
        function openDialog(settings){

            //scope.inline = typeof($location.search().inline) != 'undefined';

            function isInline(){
                return typeof($location.search().inline) != 'undefined';
            }

            // Check 401 against offline
            var modalInstance = $modal.open({
                controller: 'DialogCtrl',
                templateUrl: Config.viewsDir + 'components/dialog.html',
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
                            buttonClass: settings.buttonType? 'btn-'+ settings.buttonType : 'btn-primary',
                            needOldPassword:settings.needOldPassword,
                            createLoginAndPassword:settings.createLoginAndPassword
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
            //title, template, url, content, hasFooter, cancellable, params, closable, actionLabel, buttonType
            alert:function(message, title){
                return openDialog({
                    title:title,
                    content:message,
                    hasFooter:true,
                    cancellable:true,
                    closable:true
                }).result;
            },
            confirm:function(message, title, actionLabel, actionType){
                return openDialog({
                    title:title,
                    content:message,
                    hasFooter:true,
                    cancellable:false,
                    closable:false,
                    actionLabel:actionLabel,
                    actionType:actionType
                }).result;
            },
            confirmWithPassword:function(message, title, actionLabel, actionType){
                return openDialog({
                    title:title,
                    content:message,
                    hasFooter:true,
                    cancellable:false,
                    closable:false,
                    needOldPassword: true,
                    actionLabel:actionLabel,
                    actionType:actionType
                }).result;
            },
            createUser:function(message, title, actionLabel, actionType){
                return openDialog({
                    title:title,
                    content:message,
                    hasFooter:true,
                    cancellable:false,
                    closable:false,
                    createLoginAndPassword: true,
                    actionLabel:actionLabel,
                    actionType:actionType
                }).result;
            }
        };
    }]).controller("DialogCtrl", ['$scope', '$modalInstance', 'settings', function($scope, $modalInstance, settings){
        $scope.settings = settings;
        $scope.settings.localLogin = $scope.settings.localLogin || Config.defaultLogin;
        $scope.forms = {};
        $scope.close = function(){
            $modalInstance.dismiss('close');
        };

        $scope.ok = function(){
            if($scope.settings.createLoginAndPassword){
                $modalInstance.close( {
                    login: $scope.settings.localLogin,
                    password: $scope.settings.localPassword
                });
            }
            if($scope.settings.needOldPassword){
                $modalInstance.close( $scope.settings.oldPassword);
            }

            $modalInstance.close('ok');
        };

        $scope.cancel = function(){
            $modalInstance.dismiss('cancel');
        };

        $scope.$on('$routeChangeStart', function(){
            $modalInstance.close();
        });
    }]);
