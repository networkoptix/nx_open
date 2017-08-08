'use strict';

angular.module('webadminApp')
    .controller('ApiToolCtrl', function ($scope, mediaserver, $sessionStorage, $location, dialogs, $timeout, systemAPI) {

        $scope.Config = Config;
        $scope.session = $sessionStorage;
        $scope.allowProprietary = true;

        mediaserver.getApiMethods().then(function(data){
            $scope.apiGroups = data.data.groups;
        });

        $scope.setActiveFunction = function(group, method){
            $scope.activeGroup = group;
            $scope.activeFunction = method;
            $scope.session.method.method = method.method;
            $scope.session.method.name = group.urlPrefix + '/' + method.name;
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

        $scope.getDebugUrl = function(){
            var params = $scope.session.method.params.trim();
            if(params && params.indexOf('?')!==0 && $scope.session.method.name.indexOf('?')<0){
                params = '?' + params;
            }

            return mediaserver.debugFunctionUrl($scope.session.method.name + params);
        };

        $scope.testMethod = function(){
            var data = $scope.session.method.data;
            if(data && data !== '') {
                try {
                    data = JSON.parse(data);
                } catch (a) {
                    console.error(a);
                    dialogs.alert( 'POST-params is not a valid json object ');
                    return;
                }
            }

            var params = $scope.session.method.params;
            if(params && params !== '') {
                try {
                    params = JSON.parse(params);
                } catch (a) {
                    console.error(a);
                    dialogs.alert( 'GET-params is not a valid json object ');
                    return;
                }
            }


            mediaserver.debugFunction($scope.session.method.method, $scope.session.method.name, params, data).then(function(success){
                $scope.result.status = success.status +  ':  ' + success.statusText;
                $scope.result.result = JSON.stringify(success.data, null,  '  ');
            },function(error){
                $scope.result.status = error.status +  ':  ' + error.statusText;
                $scope.result.result =   'Error: ' + JSON.stringify(error.data, null,  '  ');
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
    });