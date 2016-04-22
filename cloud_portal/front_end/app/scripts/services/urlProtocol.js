'use strict';

angular.module('cloudApp')
    .factory('urlProtocol', ['$base64', '$location',function ($base64, $location) {

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
            open:function(systemId){

                var protocol = Config.clientProtocol;

                var system   = (systemId + '/') || 'open/';

                var params = {};

                var username = account.getEmail();
                var password = account.getPassword();
                if (username && password){
                    params.auth = $base64.encode(username + ':' + password);
                }

                var url = protocol + system + '?' + $.param(params);
                window.location.href = url;
                //window.open(url);
            },
            getSource: parseSource,
            source: parseSource()
        }
    }]);
