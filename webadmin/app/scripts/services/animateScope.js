'use strict';

angular.module('webadminApp')
    .factory('animateScope', ['$q',function ($q) {

        var animations = [];
        function Animation(scope,value,target,duration){
            this.scope = scope;
            this.value = value;
            this.targetValue = target;
            this.duration = duration;
            this.started = (new Date()).getTime();
            this.initialValue = scope[value];
            this.isFinished = false;

            this.deferred = $q.defer();

        }
        Animation.prototype.linear = function(start,stop,time,duration){
            if(time > duration) {
                time = duration;
            }
            return start + (stop-start)*time/duration;
        };
        Animation.prototype.smooth = function(start,stop,time,duration){
            if(time > duration) {
                time = duration;
            }
            var delta = (Math.sin(time/duration * Math.PI - Math.PI / 2) + 1)/2;
            return start + (stop - start) *delta ;
        };
        Animation.prototype.break = function(){
            this.isFinished = true;

            this.deferred.reject(this.scope[this.value]);
        };
        Animation.prototype.update = function() {
            if( this.isFinished){
                return;
            }
            var time = (new Date()).getTime() - this.started;
            this.scope[this.value] = this.linear(this.initialValue, this.targetValue, time, this.duration);
            this.deferred.notify(this.scope[this.value]);
            this.isFinished = time > this.duration;
            if(this.isFinished){
                this.deferred.resolve(this.scope[this.value]);
            }
        };

        var animationRunning = false;


        window.animationFrame = (function(callback) {
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

            _.forEach(animations,function(animation){
                animation.update();
                if(animation.isFinished){
                    finished.push(animation);
                }
            });

            _.forEach(finished,function(animation){ // Remove all finished animations
                animations.splice(animations.indexOf(animation),1);
            });
        }
        function animationFunction(){
            if(!animationRunning) {
                return;
            }
            process();
            if(typeof(animationHandler)!=='undefined' && animationHandler !== null && animationHandler !== false) {
                animationHandler();
            }
            window.animationFrame(animationFunction);
        }
        var defaultDuration = 1000;
        return {
            start:function(handler){
                animationRunning = true;
                animationHandler = handler;
                animationFunction();
            },
            stop:function(){
                animationRunning = false;
            },
            animate:function(scope,value,target,duration){
                if(typeof(duration) =='undefined')
                {
                    duration = defaultDuration;
                }

                var targetAnimation = _.find(animations,function(anim){ // Try to find,if there
                    return anim.scope === scope && anim.value === value;
                });
                if(!!targetAnimation){
                    targetAnimation.break();
                }
                var animation = new Animation(scope,value,target,duration);
                animations.push(animation);
                return animation.deferred.promise;
            },
            progress:function(scope,value,duration){ // Animate progress from 0 to 1
                scope[value] = 0;
                return this.animate(scope,value,1,duration);
            },
            setDuration:function(duration){
                defaultDuration = duration;
            }
        };
    }]);