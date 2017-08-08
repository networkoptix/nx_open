'use strict';

angular.module('webadminApp')
    .controller('ApiToolCtrl', ['$scope', 'mediaserver', '$sessionStorage', '$routeParams', '$location', 'dialogs', '$timeout', 'systemAPI',
    function ($scope, mediaserver, $sessionStorage, $routeParams, $location, dialogs, $timeout, systemAPI) {

        var activeApiMethod = $routeParams.apiMethod;
        $scope.Config = Config;
        $scope.session = $sessionStorage;

        if($location.search().proprietary){
            Config.allowProprietary = $location.search().proprietary;
        }
        $scope.allowProprietary = Config.allowProprietary;

        mediaserver.getApiMethods().then(function(data){
            $scope.apiGroups = data.data.groups;
            if(activeApiMethod){
                activeApiMethod = '/' + activeApiMethod;
                for(var groupIdx in $scope.apiGroups){
                    var group = $scope.apiGroups[groupIdx];
                    if(activeApiMethod.indexOf(group.urlPrefix)==0){
                        for(var funcIdx in group.functions){
                            var func = group.functions[funcIdx];
                            if(func.proprietary == 'true' && !$scope.allowProprietary){
                                break;
                            }
                            if(group.urlPrefix + '/' + func.name === activeApiMethod){
                                $scope.setActiveFunction(group,func);
                                return;
                            }
                        }
                    }
                }
            }
        });

        $scope.setActiveFunction = function(group, method, $event){
            $scope.activeGroup = group;
            $scope.activeFunction = method;
            $scope.session.method.method = method.method;
            $scope.session.method.name = group.urlPrefix + '/' + method.name;
            if($event){
                $event.preventDefault();
                $event.stopPropagation();
                $location.path('/developers/api' + group.urlPrefix + '/' + method.name, false);
            }
            return false;
        }
        $scope.clearActiveFunction = function(){
            $scope.activeGroup = null;
            $scope.activeFunction = null;
        }

        if($scope.Config.developersFeedbackForm){
            $scope.developersFeedbackForm =
                $scope.Config.developersFeedbackForm.replace("{{PRODUCT}}",encodeURIComponent(Config.productName));
        }

        /* API Test Tool */
        if(!$scope.session.method){
            $scope.session.method = {
                name:'/api/moduleInformation',
                data:'',
                params: '',
                method:'GET'
            };
        }

        $scope.result={

        };

        function cleanParams(params){
            var newParams = {};
            for(var key in params){
                if(params[key] === null || params[key].toString().trim() === ''){
                    continue;
                }
                newParams[key] = params[key].toString().trim();
            }
            return newParams;
        };
        $scope.getDebugUrl = function(){
            if($scope.session.method.method == 'GET'){
                return mediaserver.debugFunctionUrl($scope.session.method.name, cleanParams($scope.session.method.params));
            }
            return mediaserver.debugFunctionUrl($scope.session.method.name);
        };

        $scope.testMethod = function(){
            var getParams = null;
            var postParams = null;

            if($scope.session.method.method == 'GET'){
                getParams = cleanParams($scope.session.method.params);
            }else{
                postParams = cleanParams($scope.session.method.params);
            }

            mediaserver.debugFunction($scope.session.method.method,
                                      $scope.session.method.name,
                                      getParams, postParams).
                        then(function(success){
                            $scope.result.status = success.status +  ':  ' + success.statusText;
                            $scope.result.error = false;
                            $scope.result.result = JSON.stringify(success.data, null,  '  ');
                        },function(error){
                            $scope.result.status = error.status +  ':  ' + error.statusText;
                            $scope.result.error = true;
                            $scope.result.result =  JSON.stringify(error.data, null,  '  ');
                        });
        };


        /* Making window have correct size and positioning */
        var $window = $(window);
        var $header = $('header');
        var updateHeights = function() {
            var $camerasPanel = $('.cameras-panel');
            var $apiPanel = $('.api-view');
            var windowHeight = $window.height();
            var headerHeight = $header.outerHeight();

            var topAlertHeight = 0;

            var topAlert = $('.alerts-row>td');
            //after the user is notified this should not be calculated again
            if(topAlert.length && !$scope.session.mobileAppNotified){
                topAlertHeight = topAlert.outerHeight() + 1; // -1 here is a hack.
            }

            var viewportHeight = (windowHeight - headerHeight - topAlertHeight) + 'px';

            $camerasPanel.css('height',viewportHeight );
            var listWidth = $header.width() - $camerasPanel.outerWidth(true) - 1;
            $apiPanel.css('width',listWidth + 'px');
        };

        $timeout(updateHeights);

        $header.click(function() {
            //350ms delay is to give the navbar enough time to collapse
            $timeout(updateHeights,350);
        });

        $window.resize(updateHeights);
        $('html').addClass('webclient-page');
        $scope.$on('$destroy', function( event ) {
            $window.unbind('resize', updateHeights);
            $('html').removeClass('webclient-page');
        });
    }]);