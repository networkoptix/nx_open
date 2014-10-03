'use strict';

angular.module('webadminApp')
    .controller('HealthCtrl', function ($scope, $modal, $log, mediaserver) {
        $scope.healthLength = 100;//сколько точек сохраняем
        $scope.interval = 1000;// 1 секунда

        $scope.data = {
            labels: Array.apply(null, new Array( $scope.healthLength)).map(String.prototype.valueOf,''),
            datasets: [{
                label: '',
                fillColor: nocolor,
                strokeColor: nocolor,
                pointColor: nocolor,
                pointStrokeColor: nocolor,
                pointHighlightFill: nocolor,
                pointHighlightStroke: nocolor,
                show:true,
                data: Array.apply(null, new Array($scope.healthLength)).map(Number.prototype.valueOf,100)
            }]
        };

        $scope.legend = '';
        // Chart.js Options
        $scope.options =  {

            animation:false,
            scaleOverride: false,
            scaleSteps: 4,
            scaleShowLabels: false ,
            scaleLabel: "<%=value%> %",
            scaleIntegersOnly:true,


            // Sets the chart to be responsive
            responsive: true,

            ///Boolean - Whether grid lines are shown across the chart
            scaleShowGridLines : true,

            //String - Colour of the grid lines
            scaleGridLineColor : "rgba(0,0,0,.05)",

            //Number - Width of the grid lines
            scaleGridLineWidth : 1,

            //Boolean - Whether the line is curved between points
            bezierCurve : true,

            //Number - Tension of the bezier curve between points
            bezierCurveTension : 0.4,

            //Boolean - Whether to show a dot for each point
            pointDot : false,

            //Number - Radius of each point dot in pixels
            pointDotRadius : 0,

            //Number - Pixel width of point dot stroke
            pointDotStrokeWidth : 1,

            //Number - amount extra to add to the radius to cater for hit detection outside the drawn point
            pointHitDetectionRadius : 0,

            //Boolean - Whether to show a stroke for datasets
            datasetStroke : true,

            //Number - Pixel width of dataset stroke
            datasetStrokeWidth : 2,

            //Boolean - Whether to fill the dataset with a colour
            datasetFill : false,

            // Function - on animation progress
            onAnimationProgress: function(){},

            // Function - on animation complete
            onAnimationComplete: function(){},

            //String - A legend template
            legendTemplate : '<ul class="tc-chart-js-legend"><% for (var i=0; i<datasets.length; i++){%><li><span style="background-color:<%=datasets[i].strokeColor%>"></span><%if(datasets[i].label){%><%=datasets[i].label%><%}%></li><%}%></ul>'
        };

        var colors =  {
            "StatisticsCPU":        ["#3776ca"],
            "StatisticsRAM":        ["#db3ba9"],
            "StatisticsHDD":        ["#438231","#484848","#aa880b","#b25e26","#9200d1","#00d5d5","#267f00","#8f5656","#c90000"],
            "StatisticsNETWORK":    ["#ff3434", "#b08f4c", "#8484ff", "#34ff84"]
        };



        var nocolor = 'rgba(255,255,255,0)';
        function prepaireDataSets(statistics){
            var datasets = [{
                label: '',
                fillColor: nocolor,
                strokeColor: nocolor,
                pointColor: nocolor,
                pointStrokeColor: nocolor,
                pointHighlightFill: nocolor,
                pointHighlightStroke: nocolor,
                show:true,
                data: Array.apply(null, new Array($scope.healthLength)).map(Number.prototype.valueOf,100)
            }];
            for(var i=0;i<statistics.length;i++){

                var colorSet = colors[statistics[i].deviceType];
                var color = colorSet[i % colorSet.length];
                datasets.push({
                    label: statistics[i].description,
                    fillColor: color,
                    strokeColor: color,
                    pointColor: color,
                    pointStrokeColor: color,
                    pointHighlightFill: color,
                    pointHighlightStroke: color,
                    show: true,
                    data: Array.apply(null, new Array($scope.healthLength)).map(Number.prototype.valueOf,0)
                });
            }
            $scope.datasets = datasets;
            $scope.updateVisibleDatasets();
        }

        $scope.updateVisibleDatasets = function(){
            $scope.data.datasets = _.filter($scope.datasets,function(dataset){return dataset.show});
        }



        function updateStatisticsDataSets(statistics){
            var datasets = $scope.datasets;

            for(var i=0; i < datasets.length;i++){
                var dataset = datasets[i];

                var value = 0;

                var needstat = _.filter(statistics,function(stat){
                    return stat.description == dataset.label;
                });

                if(needstat && needstat.length > 0){
                    value  = needstat[0].value;
                }

                if(value>100){
                    console.log(value,dataset,needstat);
                }

                if(i!=0) {
                    dataset.data.push(value * 100);

                    if (dataset.data.length > $scope.healthLength) {
                        dataset.data = dataset.data.slice(dataset.data.length - $scope.healthLength, dataset.data.length);
                    }
                }
            }
        }

        var statisticInterval = 0;
        function updateStatistics() {
            mediaserver.statistics().then(function (r) {
                if(statisticInterval == 0){
                    // Подготовить легенды
                    prepaireDataSets(r.data.reply.statistics)
                }
                updateStatisticsDataSets((r.status==200 && r.data.error == 0) ? r.data.reply.statistics:[]);
                statisticInterval = setTimeout(updateStatistics,$scope.interval);
                return false;
            });
        }

        updateStatistics();
    });