'use strict';

angular.module('webadminApp')
    .factory('animateScope', function (mediaserver) {

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
        Animation.prototype.update = function(apply) {
            if( this.isFinished){
                return;
            }
            var time = (new Date()).getTime() - this.started;
            this.scope[this.value] = this.linear(this.initialValue, this.targetValue, time, this.duration);
            if(typeof(this.handler)!=='undefined'){
                this.handler( this.scope[this.value] );
            }
            /*if (apply && !this.scope.$$phase) {
                this.scope.$apply();
            }*/
            this.isFinished = time > this.duration;
        };

        return {
            process:function(){
                var scopes = [];
                var finished = [];

                _.forEach(animations,function(animation){
                    /*if(scopes.indexOf(animation.scope)<0){
                       scopes.push(animation.scope);
                    }*/
                    animation.update();
                    if(animation.isFinished){
                        finished.push(animation);
                    }
                });

                _.forEach(finished,function(animation){ // Remove all finished animations
                    animations.splice(animations.indexOf(animation),1);
                });

                /*_.forEach(scopes,function(scope){
                    if(!scope.$$phase) {
                        console.log(scope.$$phase);
                        scope.$apply();
                    }
                });*/
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