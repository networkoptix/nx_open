'use strict';

angular.module('cloudApp')
    .factory('dialogs', function ($http, $modal, $q, ipCookie) {

        

        function openDialog(title, controller, template, content, hasFooter, cancellable){
            // Check 401 against offline
            return $modal.open({
                controller: controller,
                templateUrl: 'views/components/dialog.html',
                keyboard:false,
                backdrop:'static',
                scope:{
                    title:title,
                    template: template,
                    hasFooter: hasFooter,
                    content:content,
                    cancellable: cancellable
                }
            });
        }

        return {
            alert:function(title, message){
                return openDialog(title, null, null, message, true, true);
            },
            confirm:function(title, message){
                return openDialog(title, null, null, message, true, false);
            },
            login:function(){
                return openDialog('Log in', 'LoginCtrl', 'views/login.html', null, false, true);
            }
        };
    });
