'use strict';

angular.module('webadminApp')
    .controller('SetupCtrl', function ($scope, mediaserver, $sessionStorage) {

        /*
            This is kind of universal wizard controller.
        */

        $scope.stepIs = function(step){
            return $scope.step == step;
        };
        $scope.finish = function(){
            $scope.next();
            console.log("close");
        };
        $scope.back = function(){
            $scope.next($scope.activeStep.back);
        };
        $scope.next = function(target){
            if(!target) {
                var activeStep = $scope.activeStep || $scope.wizardFlow[0];
                target = activeStep.next || activeStep.finish;
            }
            if($.isFunction(target)){
                return target();
            }
            $scope.step = target;
            $scope.activeStep = $scope.wizardFlow[target];
        };


        $scope.createAccount = function(){
            window.open(Config.cloud.portalRegisterUrl);
            $scope.next('cloudLogin');
        };

        $scope.learnMore = function(){
            window.open(Config.cloud.portalUrl);
        };

        $scope.wizardFlow = {
            0:{
                next:'start'
            },
            start:{
                next: "systemName"
            },
            systemName:{
                back: "start",
                next: "cloudIntro"
            },
            cloudIntro:{
                back: "systemName"
            },
            cloudLogin:{
                back: "cloudIntro",
                next: function(){
                    console.log("connect to cloud");
                    $scope.next('cloudSuccess');
                }
            },
            cloudSuccess:{
                finish: function(){
                    console.log("success");
                    $scope.next('start');
                }
            },
            cloudFailure:{
                back: "cloudLogin",
                next: "localLogin"
            },
            merge:{
                next: function(){
                    console.log("do merge");
                    $scope.next('mergeSuccess');
                }
            },
            mergeSuccess:{
                finish:function(){
                    console.log("success merge");
                    $scope.next('start');
                }
            },

            localLogin:{
                next: function(){
                    console.log("dolocal");
                    $scope.next('localSuccess');
                }
            },
            localSuccess:{
                finish:function(){
                    console.log("success local");
                    $scope.next('start');
                }
            }
        };
        $scope.next();
    });