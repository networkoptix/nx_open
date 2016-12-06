/**
 * Created by evgeniybalashov on 22/11/16.
 */
'use strict';

angular.module('webadminApp')
    .factory('nativeClient', function ($q, $log, $location, dialogs) {
        var nativeClientObject = typeof(setupDialog)=='undefined'?null:setupDialog; // Qt registered object
        var socketClientController = null;

        function parseUrl(name){
            var results = new RegExp('[\?&]' + name + '=([^&#]*)').exec(window.location.href);
            if (results==null){
                return null;
            }
            else{
                return decodeURIComponent(results[1]) || 0;
            }
        }

        var wsUri = $location.search().clientWebSocket || parseUrl('clientWebSocket');

        return {
            init:function(){
                $log.log("try to init native client");
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
            getCredentialsRaw:function(){
                $log.log("try to get credentials from native client");
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

            getCredentials:function(){
                return this.getCredentialsRaw().then(function(authObject){
                    if (typeof authObject === 'string' || authObject instanceof String) {
                        $log.log("got string from client, try to decode JSON: " + authObject);
                        try {
                            authObject = JSON.parse(authObject);
                        } catch (a) {
                            $log("could not decode JSON from string: " + authObject);
                        }
                    }
                    $log.log("got credentials from client: " + JSON.stringify(authObject, null, 4));
                    return authObject;
                });
            },
            openUrlInBrowser:function(url, title, windowFallback){
                $log.log("openUrlInBrowser", url, windowFallback);

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

                if(socketClientController){
                    var header = !title?L.dialogs.openLink:L.dialogs.openLinkWithTitle.replace("{{title}}", title);
                    dialogs.alert(url, header);
                    return $q.resolve();
                }

                if(windowFallback){
                    window.open(url);
                }
                return $q.reject();
            },
            updateCredentials:function(login,password,isCloud){
                $log.log("try to update credentials for native client");
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
                if(nativeClientObject){ // Thick client - close current window
                    $log.log("close window");
                    window.close();
                    return $q.resolve();
                }

                return $q.reject(); // No client - reject
            },
            startCamerasMode:function(){
                $log.log("try to start cameras mode in native client");
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