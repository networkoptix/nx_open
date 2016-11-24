/**
 * Created by evgeniybalashov on 22/11/16.
 */
'use strict';

angular.module('webadminApp')
    .factory('nativeClient', function ($q, $log, $location) {
        var nativeClientObject = typeof(setupDialog)=='undefined'?null:setupDialog; // Qt registered object
        var socketClientController = null;

        function parseUrl(name){
            var results = new RegExp('[\?&]' + name + '=([^&#]*)').exec(window.location.href);
            if (results==null){
                return null;
            }
            else{
                return results[1] || 0;
            }
        }

        var wsUri = $location.search().clientWebSocket || parseUrl('clientWebSocket');

        return {
            init:function(){
                if(nativeClientObject){
                    return $q.resolve({thick:true});
                }

                if(socketClientController){
                    return $q.resolve({lite:true});
                }

                if(!wsUri){
                    return $q.reject();
                }

                var deferred = $q.defer();

                var socket = new WebSocket(wsUri);
                socket.onclose = function()
                {
                    socketClientController = null;
                    $log.error("nativeClient: web channel closed");
                };
                socket.onerror = function(error)
                {
                    $log.log("nativeClient: no client websocket");
                    $log.log(error);
                    deferred.reject(error);
                };
                socket.onopen = function()
                {
                    window.channel = new QWebChannel(socket, function(channel) {
                        channel.objects.liteClientController.liteClientMode(function(arg) {
                            $log.log("nativeClient: Lite client mode enabled: " + arg)
                            socketClientController = channel.objects.liteClientController;
                            deferred.resolve({lite:true});
                        })
                    });
                };

                return deferred.promise;
            },
            getCredentials:function(){
                if(nativeClientObject && nativeClientObject.getCredentials){
                    return $q.resolve(nativeClientObject.getCredentials());
                }

                if(socketClientController && socketClientController.getCredentials){
                    var deferred = $q.defer();
                    socketClientController.getCredentials(function(result){
                        deferred.resolve(result);
                    });
                    return deferred.promise;
                }

                return $q.reject();
            },
            openUrlInBrowser:function(url, windowFallback){
                if(nativeClientObject && nativeClientObject.openUrlInBrowser){
                    return $q.resolve(nativeClientObject.openUrlInBrowser(url));
                }

                if(socketClientController && socketClientController.openUrlInBrowser){
                    var deferred = $q.defer();
                    socketClientController.openUrlInBrowser(url,function(result){
                        deferred.resolve(result);
                    });
                    return deferred.promise;
                }

                if(windowFallback){
                    window.open(url);
                }
                return $q.reject();
            },
            updateCredentials:function(login,password,isCloud){
                if(nativeClientObject && nativeClientObject.updateCredentials){
                    return $q.resolve(nativeClientObject.updateCredentials (login,password,isCloud));
                }

                if(socketClientController && socketClientController.updateCredentials){
                    var deferred = $q.defer();
                    socketClientController.updateCredentials(login,password,isCloud,function(result){
                        deferred.resolve(result);
                    });
                    return deferred.promise;
                }

                return $q.reject();
            },
            cancelDialog:function(){
                if(nativeClientObject && nativeClientObject.cancel){
                    nativeClientObject.cancel();
                }
                if(socketClientController && socketClientController.cancel){
                    var deferred = $q.defer();
                    socketClientController.cancel(function(result){
                        deferred.resolve(result);
                    });
                    return deferred.promise;
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
            startCamerasMode:function(){
                if(nativeClientObject && nativeClientObject.startCamerasMode){
                    return $q.resolve(nativeClientObject.startCamerasMode());
                }

                if(socketClientController && socketClientController.startCamerasMode){
                    var deferred = $q.defer();
                    socketClientController.startCamerasMode(function(result){
                        deferred.resolve(result);
                    });
                    return deferred.promise;
                }

                return $q.reject();
            }
        }
    });