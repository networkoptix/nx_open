/**
 * Created by evgeniybalashov on 22/11/16.
 */
'use strict';

angular.module('webadminApp')
    .factory('nativeClient', function () {
        var nativeClientObject = typeof(setupDialog)=='undefined'?null:setupDialog; // Qt registered object

        return {
            init:function(){
                if(!nativeClientObject){
                    return $q.reject();
                }
                return $q.resolve({thick:true});
            },
            getCredentials:function(){
                if(!(nativeClientObject && nativeClientObject.getCredentials)){
                    return $q.reject();
                }
                return $q.resolve(nativeClientObject.getCredentials());
            },
            openUrlInBrowser:function(url, windowFallback){
                if(!(nativeClientObject && nativeClientObject.openUrlInBrowser)){
                    if(windowFallback){
                        window.open(url);
                    }
                    return $q.reject();
                }
                return $q.resolve(nativeClientObject.openUrlInBrowser(url));
            },
            updateCredentials:function(login,password,isCloud){
                if(!(nativeClientObject && nativeClientObject.updateCredentials)){
                    return $q.reject();
                }
                return $q.resolve(nativeClientObject.updateCredentials (login,password,isCloud));
            },
            cancelDialog:function(){
                if(!nativeClientObject){
                    return $q.reject();
                }
                if(nativeClientObject.cancel){
                    nativeClientObject.cancel();
                }
                return this.closeDialog();
            },
            closeDialog:function(){
                if(!nativeClientObject){
                    return $q.reject();
                }
                $log.log("close dialog");
                window.close();
                return $q.resolve();
            },
            liteClientMode:function(){
                if(!nativeClientObject){
                    return $q.reject();
                }

            },
            startCamerasMode:function(){
                if(!nativeClientObject){
                    return $q.reject();
                }

            }
        }
    });