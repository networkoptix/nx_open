'use strict';

angular.module('cloudApp')
    .factory('dialogs', function ($http, $modal, $q) {

        

        function openDialog(title, template, content, hasFooter, cancellable){
            // Check 401 against offline

            return $modal.open({
                controller: 'DialogCtrl',
                templateUrl: 'views/components/dialog.html',
                keyboard:false,
                backdrop:'static',
                resolve: {
                    settings:function(){
                        return {
                            title:title,
                            template: template,
                            hasFooter: hasFooter,
                            content:content,
                            cancellable: cancellable
                        };
                    }
                }
            });
        }

        return {
            alert:function(title, message){
                return openDialog(title, null, message, true, true);
            },
            confirm:function(title, message){
                return openDialog(title, null, message, true, false);
            },
            login:function(){
                return openDialog('Log in', 'views/login.html', null, false, true);
            }
        };
    }).controller("DialogCtrl",function($scope, $modalInstance,settings){
        console.log("DialogCtrl",settings);
        $scope.settings = settings;

        $scope.close = function(){
            $modalInstance.dismiss('close');
        };

        $scope.ok = function(){
            $modalInstance.close('ok');
        };

        $scope.cancel = function(){
            $modalInstance.dismiss('cancel');
        };

        $scope.$on('$routeChangeStart', function(){
            $modalInstance.close();
        });
    });
