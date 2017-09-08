'use strict';

angular.module('cloudApp')
    .directive('header',['dialogs', 'cloudApi', 'account', '$location', '$route', '$poll',
    function (dialogs, cloudApi, account, $location, $route, $poll) {
        return {
            restrict: 'E',
            templateUrl: Config.viewsDir + 'components/header.html',
            link:function(scope/*,element,attrs*/){
                scope.inline = typeof($location.search().inline) != 'undefined';

                if(scope.inline){
                    $('body').addClass('inline-portal');
                }

                scope.login = function(){
                    dialogs.login();
                };
                scope.logout = function(){
                    account.logout();
                };

                function isActive(val){
                    var currentPath = $location.path();
                    if(currentPath.indexOf(val)<0){ // no match
                        return false;
                    }
                    return true;
                };
                scope.active = {};
                function updateActive(){
                    scope.active.register = isActive('/register');
                    scope.active.view = isActive('/view');
                    scope.active.settings = $route.current.params.systemId && !isActive('/view');
                }
                function updateActiveSystem(){
                    if(!scope.systems){
                        return;
                    }
                    scope.activeSystem = _.find(scope.systems,function(system){
                        return $route.current.params.systemId == system.id;
                    });
                    if(scope.singleSystem){ // Special case for a single system - it always active
                        scope.activeSystem = scope.systems[0];
                    }
                }
                function updateSystems(){
                    return cloudApi.systems().then(function(result){
                        scope.systems = cloudApi.sortSystems(result.data);
                        scope.singleSystem = scope.systems.length == 1;
                        scope.systemCounter = scope.systems.length;
                        updateActiveSystem();
                    });
                }
                function delayedUpdateSystems(){
                    var pollingSystemsUpdate = $poll(updateSystems,Config.updateInterval);

                    scope.$on('$destroy', function( event ) {
                        $poll.cancel(pollingSystemsUpdate);
                    } );
                }
                updateActive();
                account.get().then(function(account){
                    scope.account = account;
                    updateSystems().then(delayedUpdateSystems);

                    $('body').removeClass('loading');
                    $('body').addClass('authorized');
                },function(){
                    $('body').removeClass('loading');
                    $('body').addClass('anonymous');
                });


                scope.$on('$locationChangeSuccess', function(next, current) {
                    if($route.current.params.systemId){
                        if(!scope.systems){
                            updateSystems();
                        }else{
                            updateActiveSystem();
                        }
                    }
                    updateActive();
                });

            }
        };
    }]);
