'use strict';

angular.module('nxCommon')
    .factory('animateScope', ['$q',function ($q) {

        var animations = [];

        var updationLimit = null;// Limit animations to stop process at some point
        var defaultDuration = 1000;
        var defaultScope = null;
        var defaultAnimation = "linear";
        function Animation(scope,value,target,duration,dependency){
            this.scope = scope;
            this.value = value;
            this.targetValue = target;
            this.duration = duration;
            this.started = (new Date()).getTime();
            this.initialValue = scope[value];
            this.isFinished = false;
            this.dependency = dependency || defaultAnimation;

            this.deferred = $q.defer();

        }

        Animation.prototype.interpolate = function(start,stop,time,duration, dependency) {
            if(duration == 0 || time >= duration){
                return stop;
            }
            switch(dependency){
                case "smooth":
                    return this.smooth(start,stop,time,duration);
                case "fading":
                    return this.fading(start,stop,time,duration);
                case "dryResistance":
                    return this.dryResistance(start,stop,time,duration);
                case "linear":
                default:
                    return this.linear(start,stop,time,duration);
            }
        };

        Animation.prototype.linear = function(start,stop,time,duration){
            var result = start + (stop-start)*time/duration;
            if(Number.isNaN(result)){
                console.error('linear-error',result,start,stop,time,duration);
                throw "Animation value is not a number";
            }
            return result;
        };
        Animation.prototype.smooth = function(start,stop,time,duration){
            var delta = (Math.sin(time/duration * Math.PI - Math.PI / 2) + 1)/2;
            var result =start + (stop - start) * delta;
            if(Number.isNaN(result)){
                throw "Animation value is not a number";
            }
            return  result;
        };


        /*
         * Dry resistance is a physics model:
         *
         * x(t)=x0 + v0*t - k*t^2/2;
         * k is a resistance koefficient, constant
         *
         * We have to move to stop by duration, so
         * k = 2*(stop-start)/duration^2;
         * v0 = 2*(stop-start)/duration
         *
         * x(t) = x0 + (stop-start)* (2* time/duration - (time/duration)^2)
         *
         * !!! Dry resistance allows to slow down after linear movement
         * if distance is exactly twice smaller or duration is twice larger
         */
        Animation.prototype.dryResistance = function(start,stop,time,duration){
            var proportion = time/duration;
            var delta = proportion * (2 - proportion);
            var result =start + (stop - start) * delta;
            return result;
        };

        Animation.prototype.fading = function(start,stop,time,duration){
            var delta = Math.sin(time/duration * Math.PI / 2);
            return  start + (stop - start) *delta;
        };
        Animation.prototype.breakAnimation = function(){
            this.isFinished = true;

            this.deferred.reject(this.scope[this.value]);
        };
        Animation.prototype.update = function() {
            if(this.isFinished){
                return;
            }
            var time = (new Date()).getTime() - this.started;
            this.scope[this.value] = this.interpolate(this.initialValue, this.targetValue, time, this.duration,this.dependency);

            this.deferred.notify(this.scope[this.value]);
            this.isFinished = time >= this.duration;

            if(this.isFinished){
                this.deferred.resolve(this.scope[this.value]);
            }
        };

        var animationRunning = false;


        window.animationFrame = (function() {
            return window.requestAnimationFrame ||
                window.webkitRequestAnimationFrame ||
                window.mozRequestAnimationFrame ||
                window.oRequestAnimationFrame ||
                window.msRequestAnimationFrame ||
                function(callback) {
                    window.setTimeout(callback, 1000/50); // target: 50fps
                };
        })();

        var animationHandler = null;

        function process() {
            var finished = [];

            var scopes = [];

            _.forEach(animations,function(animation){
                animation.update();
                if(scopes.indexOf(animation.scope)<0 && animation.scope !== defaultScope) {
                    scopes.push(animation.scope);
                }
                if(animation.isFinished){
                    finished.push(animation);
                }
            });

            _.forEach(finished,function(animation){ // Remove all finished animations
                animations.splice(animations.indexOf(animation),1);
            });

            _.forEach(scopes,function(scope){
                scope.$apply();
            });
        }
        function animationFunction(digestContext){
            if(!animationRunning) {
                return;
            }
            process();
            if(typeof(animationHandler)!=='undefined' && animationHandler !== null && animationHandler !== false) {
                animationHandler();
            }
            /*if(defaultScope.$root.$$phase && !digestContext ){
                console.error('wrong phase',defaultScope.$root.$$phase);
            }
            if(defaultScope !== null && !digestContext) {
                defaultScope.$apply();
            }*/
            if(updationLimit!==0) {
                window.animationFrame(animationFunction);
                if(updationLimit) {
                    updationLimit--;
                }
            }
        }
        return {
            start:function(handler){
                animationRunning = true;
                animationHandler = handler;
                animationFunction(true);
            },
            stop:function(){
                animationRunning = false;
            },
            process:function(){
                process();
            },
            animating:function(scope,value){
                var targetAnimation = _.find(animations,function(anim){ // Try to find,if there
                    return anim.scope === scope && anim.value === value && !anim.isFinished;
                });
                return targetAnimation || false;
            },
            animate:function(scope,value,target,dependency,duration){

                if(typeof(duration) === 'undefined')
                {
                    duration = defaultDuration;
                }

                if(Number.isNaN(value)){
                    throw 'Animation target is not a number';
                }

                var targetAnimation = this.animating(scope, value);
                if(targetAnimation){
                    targetAnimation.breakAnimation();
                }
                var animation = new Animation(scope,value,target,duration,dependency);
                animations.push(animation);
                return animation.deferred.promise;
            },
            progress:function(scope,value,dependency,duration){ // Animate progress from 0 to 1
                scope[value] = 0;
                return this.animate(scope,value,1,dependency,duration);
            },
            setDuration:function(duration){
                defaultDuration = duration;
            },
            setScope:function(scope){
                defaultScope = scope;
            },
            stopScope:function(scope){
                var targetAnimation = _.find(animations,function(anim){ // Try to find,if there

                    return anim.scope === scope;
                });
                if(targetAnimation){
                    targetAnimation.breakAnimation();
                } 
            },
            stopHandler:function(handler){
                if(animationHandler === handler){
                    animationHandler = null;
                }
            }
        };
    }]);