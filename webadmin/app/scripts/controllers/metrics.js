'use strict';

angular.module('webadminApp')
    .controller('MetricsCtrl', ['$scope', '$location', 'camerasProvider', 'systemAPI', '$q', '$poll',
    function ($scope, $location, camerasProvider, systemAPI, $q, $poll) {
        $scope.Config = Config;
        var showAll = $location.search().all;

        $scope.serversMetrics = {};
        function updateCameras(){
            function cameraSorter(camera){
                var sortCamera = Config.metrics.statusOrder[camera.status] || 0;
                var server = $scope.camerasProvider.getServer(camera.parentId);
                var sortServer = Config.metrics.statusOrder[server.status] || 0;
                return sortCamera*10 + sortServer;
            }
            $scope.cameras = _.sortBy($scope.camerasProvider.camerasList, cameraSorter);
            console.log($scope.cameras);
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
                if(!showAll && Config.metrics.hide[key]){ // Ignore
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

        function updateMetricsTableStructure(){
            // here we turn retrieve columns from serverStat (flattened)
            $scope.metricsColumns = {};
            for(var serverId in $scope.serversMetrics){
                var stats = $scope.serversMetrics[serverId];
                for(var key in stats){
                    $scope.metricsColumns[key] = stats[key].description;
                }
            }
            // TODO: later - check in config what order do we need for columns
        }

        function updateMediaServers(){
            $scope.mediaServers = $scope.camerasProvider.getMediaServers();

            function requestServerStats(server){
                return systemAPI.getServerMetrics(server.id).then(function (data) {
                    $scope.serversMetrics[server.id] = flattenMetrics(data.data.reply);
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
            $q.all(requests).finally(function(){ // Then all requests are done - check if we should update table
               updateMetricsTableStructure();
            });
        }



        function formatPercentValue(value){
            var toreturn = value * 100; // Turn to percents
            if(toreturn>10) { // 0.29405167545054756 - typical CPU/RAM usage
                return toreturn.toFixed(0); // No need to have digits after decimal point
            }
            if(toreturn>1) { //0.02749664306640625 - low CPU/RAM usage
                return toreturn.toFixed(1); // Only one digit after decimal point after
            }
            return toreturn.toFixed(2); // Two digits after decimal point for other cases
        }
        function groupStatistics(stats){
            /*
                    {
                        "description": "CPU",
                        "deviceFlags": 0,
                        "deviceType": "StatisticsCPU",
                        "value": 0.7379624936644704
                    },
                    {
                        "description": "RAM", // This is name
                        "deviceFlags": 0,
                        "deviceType": "StatisticsRAM", // This is family - use as column
                        "value": 0.29405167545054756 // This is value
                    },

                    output:
                    {
                        "StatisticsCPU":{
                            "CPU":0.7379624936644704
                        },
                         "StatisticsRAM":{
                            "RAM":0.29405167545054756
                         }
                    }
                    * */
            var statistics = {};
            for(var i in stats){
                var data = stats[i];
                if(typeof(statistics[data.deviceType]) === 'undefined'){
                    statistics[data.deviceType] = {};
                }
                statistics[data.deviceType][data.description] = formatPercentValue(data.value);
            }
            return statistics;
        }

        $scope.serversStatistics = {};
        function updateStatisticsTableStructure(){
        // here we turn retrieve columns from serverStat (flattened)
            $scope.statisticsColumns = {};
            for(var serverId in $scope.serversStatistics){
                var stats = $scope.serversStatistics[serverId];
                for(var group in stats){
                    $scope.statisticsColumns[group] = group.replace('Statistics','');
                }
            }
            // TODO: later - check in config what order do we need for columns
        }
        function updateStatistics() {
            function requestServerStats(server){
                return systemAPI.getServerStatistics(server.id).then(function(r){
                    $scope.serversStatistics[server.id] = groupStatistics(r.data.reply.statistics);
                });
            }

            var requests = []; // Array to track all metrics requests

            for(var key in $scope.mediaServers) {
                var server = $scope.mediaServers[key];
                requests.push(requestServerStats(server));
            }


            return $q.all(requests).finally(function(){
                // Here we build columns for statistics
                updateStatisticsTableStructure();
            });
        }


        $scope.camerasProvider = camerasProvider.getProvider(systemAPI);

        $scope.camerasProvider.reloadTree().then(function(){
            $scope.$watch('camerasProvider.camerasList', updateCameras, true);
            $scope.$watch('camerasProvider.mediaServers', updateMediaServers, true);
            updateStatistics();

            $scope.camerasProvider.startPoll(); // Watching servers and cameras
            var liveMetricsPoll = $poll(updateStatistics, Config.metrics.liveMetricsUpdate); // Watching CPU, RAM, etc

            $scope.$on('$destroy', function( event ) {
                $scope.camerasProvider.stopPoll();
                $poll.cancel(liveMetricsPoll);
            });
        },function(error){
            console.error(error);
        });
    }]);
