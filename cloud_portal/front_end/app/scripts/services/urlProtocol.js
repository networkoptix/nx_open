'use strict';

angular.module('cloudApp')
    .factory('urlProtocol', ['$base64', '$location', 'account', '$q',
    function ($base64, $location, account, $q) {

        function parseSource() {
            var search = $location.search();
            var source = {
                from: search.from || 'portal',
                context: search.context || 'none'
            };
            source.isApp = (source.from === 'client' || source.from === 'mobile' );
            return source;
        }

        return {
            generateLink:function(linkSettings){
                linkSettings = linkSettings || {};
                var settings = {
                    native: true,
                    from: 'portal',    // client, mobile, portal, webadmin
                    context: null,
                    command: 'client', // client, cloud, system
                    systemId: null,
                    action: null,
                    actionParameters: null, // Object with parameters
                    auth: true // true for request, null for skipping, string for specific value
                };

                if(linkSettings.systemId){
                    settings.command = 'systems';
                }

                $.extend(settings,linkSettings);

                var protocol = settings.native?L.clientProtocol:location.protocol;
                var host = settings.native?L.clientDomain:location.host;

                var getParams = $.extend({},settings.actionParameters);

                if(settings.from){
                    getParams.from = settings.from;
                }
                if(settings.auth){
                    getParams.auth = settings.auth;
                }

                if(settings.context){
                    getParams.context = settings.context;
                }

                var url = protocol + '//' + host + '/' + settings.command + '/';
                if(linkSettings.systemId){
                    url += linkSettings.systemId + '/';
                }
                if(linkSettings.action){
                    url += linkSettings.action;
                }
                url += '?' + $.param(getParams);
                console.log("generated link", url);
                return url;

            },
            getLink:function(linkSettings){
                var defer = $q.defer();
                var self = this;
                account.authKey().then(function(authKey){
                    linkSettings.auth = authKey;
                    defer.resolve(self.generateLink(linkSettings));
                },function(no_account){
                    defer.resolve(self.generateLink(linkSettings));
                    // defer.reject(null);
                });
                return defer.promise;
            },
            open:function(systemId){
                var result = $q.defer();
                this.getLink({
                    systemId: systemId
                }).then(function(link){
                    link = link.replace(/&/g,'&&'); // This is hack,
                    // Google Chrome for mac has a bug - he looses one ampersand which brakes the link parameters
                    // Here we duplicate ampersands to keep one of them
                    // Dear successor, if you read this - plese, check if the bug was fixed in chrome and remove this
                    // ugly thing!
                    // see CLOUD-716 for more information
                    window.protocolCheck(link, function () {
                        result.reject(L.errorCodes.noClientDetected);
                    },function(){
                        result.resolve();
                    });
                });
                return result.promise;
            },
            getSource: parseSource,
            source: parseSource()
        }
    }]);
