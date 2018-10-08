'use strict';

angular.module('webadminApp')
    .controller('MetricsCtrl', ['$scope', '$location', 'camerasProvider', 'systemAPI', '$q',
    function ($scope, $location, camerasProvider, systemAPI, $q) {
        $scope.config = Config;

        $scope.serverStats = {};
        function updateCameras(){
            $scope.cameras = $scope.camerasProvider.cameras;
        }

        function flattenMetrics(metrics, group, groupDescription){
            function joinString(a, b, separator){
                if(!a){
                    return b;
                }
                if(!b) {
                    return a;
                }
                return a + separator + b;
            }
            group = group || '';
            groupDescription = groupDescription || '';
            // Turn nested object with all metrics data into plain list
            /*
            supported format:
            "tcpConnections": {
                "description": "Opened TCP connections",
                "value": {
                    "hls": {
                        "description": "Amount of opened HLS connections",
                        "value": 0
                    },
                    ...
                }
                ...
            }

            output:

            {
                "tcpConnections.hls": {
                    "groupDescription": "Opened TCP connections",
                    "description": "Amount of opened HLS connections",
                    "value": 0
                }
            }

            * */
            var values = {};

            for(var key in metrics){
                if(Config.metrics.hide[key]){ // Ignore
                    continue;
                }
                var metric = metrics[key];
                var name = joinString(group, key, '.');

                if(typeof(metric.value) === 'undefined') { // Unconventional metrics like p2p
                    values[name] = {
                        description: joinString(groupDescription, key, '/'),
                        value: metric
                    };
                    continue;
                }

                var description =  metric.description; // joinString(groupDescription, metric.description, '/');
                if(_.isObject(metric.value)){
                    var metricValues = flattenMetrics(metric.value, name, description);
                    _.extend(values, metricValues);
                }else{
                    values[name] = {
                        description: description,
                        value: metric.value
                    };
                }
            }
            return values;
        }

        function updateTableStructure(){
            // here we turn retrieve columns from serverStat (flattened)
            $scope.columns = {};
            for(var serverId in $scope.serverStats){
                var stats = $scope.serverStats[serverId];
                for(var key in stats){
                    $scope.columns[key] = stats[key].description;
                }
            }
            // TODO: later - check in config what metrics to show and in what order
        }

        function updateMediaServers(){
            console.log("updateMediaServers");

            $scope.mediaServers = $scope.camerasProvider.getMediaServers();

            function requestServerStats(server){
                return systemAPI.getServerMetrics(server.id).then(function (data) {
                    console.log('flattenMetrics', data.data.reply);
                    $scope.serverStats[server.id] = flattenMetrics(data.data.reply);
                }, function (error) {
                    server.isNotAvailable = true;
                    // TODO: pass error information to the interface
                });
            }
            var requests = []; // Array to track all metrics requests
            for(var key in $scope.mediaServers){
                var server = $scope.mediaServers[key];
                if(server.status !== 'Online'){ // If server is not online - do not update its data
                    server.isOffline = true; // Flag for GUI to render line disabled
                    continue;
                }
                requests.push(requestServerStats(server));
            }
            $q.all(requests).then(function(){ // Then all requests are done - check if we should update table
               updateTableStructure();
            });
        }

        //1. start watching servers and cameras

        $scope.camerasProvider = camerasProvider.getProvider(systemAPI);



        //2. build columns description
        //3. build table to send to the template
        //4. request getCameras, render their statistics
        //5. Request metrics - CPU, RAM
        $scope.camerasProvider.reloadTree().then(function(){
            $scope.$watch('camerasProvider.cameras', updateCameras, true);
            $scope.$watch('camerasProvider.mediaServers', updateMediaServers, true);
            $scope.camerasProvider.startPoll();
            $scope.$on('$destroy', function( event ) {
                $scope.camerasProvider.stopPoll();
            });
        },function(error){
            console.error(error);
        });
    }]);
