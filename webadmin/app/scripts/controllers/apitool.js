'use strict';

angular.module('webadminApp')
    .controller('ApiToolCtrl', ['$scope', 'mediaserver', '$sessionStorage', '$routeParams',
                                '$location', '$timeout', 'systemAPI',
    function ($scope, mediaserver, $sessionStorage, $routeParams,
              $location, $timeout, systemAPI) {

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
            }
            var regex = /(<([^>]+)>)/ig; // Regex to clean tags from description
            _.each($scope.apiGroups,function(group){
                _.each(group.functions,function(func){
                    func.url = group.urlPrefix + '/' + func.name;
                    func.searchLabel = func.url + func.caption;
                    func.description.xml = func.description.xml.replace( /<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1' );
                    _.each(func.params,function(param){
                        // Detecting type for param
                        param.description.xml = param.description.xml.replace( /<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1' );
                        param.formType = param.type;
                        if(param.name === 'cameraId'){
                            param.formType = 'camera';
                        }

                        if(param.type === 'enum' || param.type === 'flags'){
                            if(!param.values){ // No values for enum - fallback to text
                                param.formType = 'text';
                            }else {
                                _.each(param.values, function (value) {
                                    value.label = value.name;
                                    if (value.description.xml) {
                                        value.description.xml = value.description.xml.replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1');
                                        if (value.description.xml.trim() != '') {
                                            value.label += ': ' + value.description.xml.replace(regex, ''); // Clean tags
                                        }
                                    }
                                });
                                if (param.optional && (func.method == 'GET' || func.method == 'POST')) {
                                    param.values.unshift({label: '', name: null});
                                }
                            }
                        }

                        if(['object',
                            'array',
                            'stringArray',
                            'binary'].indexOf(param.type)!=-1){
                            // Cannot edit in form
                            param.formType='info';
                        }

                        if(['float',
                            'uuid',
                            'objectJson',
                            'arrayJson',
                            'flags'].indexOf(param.type)!=-1){
                            // Fallback to text
                            param.formType='text';
                        }

                        if(param.name.match(/[{}\.\[\]]/g)){
                            // Part of object - not supported
                            param.formType = 'info';
                        }
                    });

                    if(func.url == activeApiMethod){ // Check if we should select this method (checking url parameter)
                        if(func.proprietary && !$scope.allowProprietary){
                            return; // proprietary methods are forbidden
                        }
                        $scope.setActiveFunction(group, func);
                    }
                });
            });
        });

        systemAPI.getCameras().then(function(data){
            $scope.cameras = _.sortBy(data.data,function(camera){
                camera.searchLabel = camera.id + '|' + camera.name + "|" + camera.physicalId;
                camera.id = camera.id.replace("{","").replace("}","");
                return camera.name;
            });
            $scope.cameraIds = _.indexBy($scope.cameras,function(camera){
                return camera.id;
            });
        });

        $scope.setActiveFunction = function(group, method, $event){
            if($scope.activeFunction == method){
                return;
            }
            $scope.result={};
            $scope.activeGroup = group;
            $scope.activeFunction = method;
            $scope.apiMethod.method = method.method;
            $scope.apiMethod.name = method.url;
            $scope.apiMethod.permissions = method.permissions;
            $scope.apiMethod.params = {};
            _.each(method.params,function(param){
                $scope.apiMethod.params[param.name] = null;
            });
            if($event){
                $event.preventDefault();
                $event.stopPropagation();
                $location.path('/developers/api' + method.url, false);
            }
            return false;
        };
        $scope.clearActiveFunction = function($event){
            $scope.activeGroup = null;
            $scope.activeFunction = null;
            if($event){
                $event.preventDefault();
                $event.stopPropagation();
                $location.path('/developers/api', false);
            }
            return false;
        };

        if($scope.Config.developersFeedbackForm){
            $scope.developersFeedbackForm =
                $scope.Config.developersFeedbackForm.replace("{{PRODUCT}}",encodeURIComponent(Config.productName));
        }

        /* API Test Tool */
        if(!$scope.apiMethod){
            $scope.apiMethod = {
                name:'/api/moduleInformation',
                data:'',
                params: '',
                method:'GET',
                addCredentials: false,
                login: 'login',
                password: 'password'
            };
        }

        $scope.result = {};

        function cleanParams(params){
            var newParams = {};
            for(var key in params){
                if(params[key] === null || params[key].toString().trim() === ''){
                    continue;
                }
                if(params[key] instanceof Date){
                    newParams[key] = params[key].getTime();
                    continue;
                }
                newParams[key] = params[key];
            }
            return newParams;
        }
        $scope.getDebugUrl = function(){
            if($scope.apiMethod.method == 'GET'){
                return mediaserver.debugFunctionUrl($scope.apiMethod.name,
                                                    cleanParams($scope.apiMethod.params),
                                                    $scope.apiMethod.addCredentials && $scope.apiMethod);
            }
            return mediaserver.debugFunctionUrl($scope.apiMethod.name, null,
                                                $scope.apiMethod.addCredentials && $scope.apiMethod);
        };

        $scope.getPostData = function(){
            return JSON.stringify(cleanParams($scope.apiMethod.params), null,  '  ');
        };

        $scope.testMethod = function(){
            var getParams = null;
            var postParams = null;
            $scope.result = {};

            if($scope.apiMethod.method == 'GET'){
                getParams = cleanParams($scope.apiMethod.params);
            }else{
                postParams = cleanParams($scope.apiMethod.params);
            }
            function processImage(result){
                $scope.result.contentType = result.headers('content-type');
                var blb = new Blob([result.data], {type: $scope.result.contentType});
                $scope.result.image = (window.URL || window.webkitURL).createObjectURL(blb);
                $scope.result.status = result.status +  ':  ' + result.statusText;
            }

            function formatResult(result){
                $scope.result.status = result.status +  ':  ' + result.statusText;
                if (typeof result.data === 'string' || result.data instanceof String){
                    $scope.result.result = vkbeautify.xml(result.data, 4);
                }else{
                    $scope.result.result = JSON.stringify(result.data, null,  '  ');
                }
            }
            mediaserver.debugFunction($scope.apiMethod.method,
                                      $scope.apiMethod.name,
                                      getParams, postParams).
                        then(function(success){
                            $scope.result.error = false;

                            var contentType = success.headers('content-type');
                            if(contentType.indexOf("image/") == 0){
                                mediaserver.debugFunction($scope.apiMethod.method,
                                                          $scope.apiMethod.name,
                                                          getParams, postParams, true).then(processImage);
                                return;
                            }
                            formatResult(success);
                        },function(error){
                            $scope.result.error = true;
                            formatResult(error);
                        });
        };

        $scope.openDate = {};
        $scope.openDatePicker = function($event, param_name) {
            $event.preventDefault();
            $event.stopPropagation();
            $scope.openDate[param_name] = true;
        };


        /* Making window have correct size and positioning */

        function initResizing(){
            var $window = $(window);
            var $header = $('header');
            var updateHeights = function() {
                var $camerasPanel = $('.cameras-panel');
                var $apiPanel = $('.api-view');
                var windowHeight = $window.height();
                var headerHeight = $header.outerHeight();

                var viewportHeight = (windowHeight - headerHeight) + 'px';

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
        }

        if($('.cameras-panel').length){
            initResizing();
        }
    }]);
