'use strict';

angular.module('webadminApp')
    .factory('animateScope', function () {

        var animations = [];
        function Animation(scope,value,target,duration,handler){
            this.scope = scope;
            this.value = value;
            this.targetValue = target;
            this.duration = duration;
            this.started = (new Date()).getTime();
            this.initialValue = scope[value];
            this.isFinished = false;
            this.handler = handler;
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
        Animation.prototype.update = function() {
            if( this.isFinished){
                return;
            }
            var time = (new Date()).getTime() - this.started;
            this.scope[this.value] = this.linear(this.initialValue, this.targetValue, time, this.duration);
            if(typeof(this.handler)!=='undefined'){
                this.handler( this.scope[this.value] );
            }
            this.isFinished = time > this.duration;
        };

        return {
            process:function(){
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
            },
            animate:function(scope,value,target,duration,handler){

                var targetAnimation = _.find(animations,function(anim){ // Try to find,if there
                    return anim.scope === scope && anim.value === value;
                });
                if(!!targetAnimation){
                    targetAnimation.isFinished = true; // Disable old animation
                }
                animations.push(new Animation(scope,value,target,duration,handler));
            }
        };
    });