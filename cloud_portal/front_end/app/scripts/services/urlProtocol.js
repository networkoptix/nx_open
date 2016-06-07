'use strict';

angular.module('cloudApp')
    .factory('urlProtocol', ['$base64', '$location', 'account', function ($base64, $location, account) {

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
                    settings.command = 'system';
                }

                $.extend(settings,linkSettings);

                var protocol = settings.native?Config.clientProtocol:location.protocol;
                var host = settings.native?Config.nativeDomain:location.host;

                if(settings.auth === true){ // TODO: remove for cloud request later
                    var username = account.getEmail();
                    var password = account.getPassword();
                    settings.auth = $base64.encode(username + ':' + password);
                }

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
                return url;

            },
            open:function(systemId){
                window.location.href = this.generateLink({
                    systemId: systemId
                });
            },
            getSource: parseSource,
            source: parseSource()
        }
    }]);
