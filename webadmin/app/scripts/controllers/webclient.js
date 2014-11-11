'use strict';

angular.module('webadminApp').controller('WebclientCtrl', function ($scope,mediaserver) {

    $scope.cameras = {};

    $scope.selectCamera = function(activeCamera){
        $scope.activeCamera = activeCamera;

        $scope.acitveVideoSource = [
            {
                src:'/media/'+ activeCamera.physicalId+'.webm',
                type:'video/webm'
            },
            {
                src:'/media/'+ activeCamera.physicalId+'.mp4',
                type:'video/mp4'
            },
            {
                src:'/hls/'+ activeCamera.physicalId+'.m3u'
            },
            {
                src:'/hls/'+ activeCamera.physicalId+'.m3u8'
            },
            {
                src:'/hls/'+ activeCamera.physicalId+'.mpegts'
            },
            {
                src:'/hls/'+ activeCamera.physicalId+'.3gp'
            },
            {
                src: '/hls/' + activeCamera.physicalId + '.mpjpeg'
            }
        ];

        return false;
    };
    function getCamerasForServer(id){
        mediaserver.getCameras(id).then(function(data){

            console.log("getCameras",id,data.data);
            $scope.cameras[id] = data.data;
        },function(){
            alert("network problem");
        })
    }
    mediaserver.getMediaServers().then(function(data){
        console.log("getMediaServers",data.data);
        $scope.mediaServers = data.data;

        for(var i=0;i < data.data.length;i++){
            getCamerasForServer(data.data[i].id);
        }
    },function(){
        alert("network problem");
    })
});
