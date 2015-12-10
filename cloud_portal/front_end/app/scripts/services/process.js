'use strict';


angular.module('cloudApp')
    .factory('process', function () {
        return {
            init:function(caller){
                return {
                    success:false,
                    error:false,
                    processing: false,
                    errorData: null,
                    run:function(){
                        console.log("run process",this);
                        this.processing = true;
                        var self = this;
                        return caller().then(function(){
                            self.success = true;
                            self.processing = false;
                        },function(error){
                            console.error("process error",error);
                            self.error = true;
                            self.errorData = error;
                            self.errorMessage = error.data.errorText;
                            self.processing = false;
                        });
                    }
                }
            }
        }
    });
