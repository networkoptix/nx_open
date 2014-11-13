'use strict';

angular.module('webadminApp').controller('ViewCtrl', function ($scope,$location,$routeParams,mediaserver) {

    $scope.cameraId = $routeParams.cameraId;
    $scope.cameras = {};
    $scope.activeCamera = null;

    $scope.selectCamera = function(activeCamera){

        $location.path('/view/' + activeCamera.id, false);

        $scope.activeCamera = activeCamera;

        $scope.acitveVideoSource = [
            { src:'/media/'+ activeCamera.physicalId+'.webm', type:'video/webm' },
            { src:'/media/'+ activeCamera.physicalId+'.mp4', type:'video/mp4' },
            { src:'/hls/'+ activeCamera.physicalId+'.m3u' },
            { src:'/hls/'+ activeCamera.physicalId+'.m3u8' },
            { src:'/hls/'+ activeCamera.physicalId+'.mpegts' },
            { src:'/hls/'+ activeCamera.physicalId+'.3gp' },
            { src: '/hls/' + activeCamera.physicalId + '.mpjpeg' }
        ];


        return false;
    };

    function extractDomain(url) {
        url = url.split('/')[2] || url.split('/')[0];
        return url.split(':')[0].split('?')[0];
    }
    function getCameras(){
        mediaserver.getCameras().then(function(data){
            var cameras = data.data;


            function cameraSorter(camera){
                camera.url = extractDomain(camera.url);

                if(camera.id === $scope.cameraId){
                    $scope.selectCamera(camera);
                }

                var num = 0;
                try {
                    var addrArray = camera.url.split('.');
                    for (var i = 0; i < addrArray.length; i++) {
                        var power = 3 - i;
                        num += ((parseInt(addrArray[i]) % 256 * Math.pow(256, power)));
                    }
                    if(isNaN(num)){
                        throw num;
                    }
                    num = num.toString(16);
                    if(num.length<8) {
                        num = '0' + num;
                    }
                } catch (a) {
                    num = camera.url;
                }
                return camera.name + '__' + num;
            }

            //1. split cameras with groupId and without
            var cams =_.partition (cameras,function(cam){
                return cam.groupId==='';
            });

            //2. sort groupedCameras
            cams[1] = _.sortBy(cams[1],cameraSorter);

            //3. group groupedCameras by groups ^_^
            cams[1] = _.groupBy(cams[1],function(cam){
                return cam.groupId;
            });

            //4. Translate into array
            cams[1] = _.values(cams[1]);

            //5. Emulate cameras by groups
            cams[1] = _.map(cams[1],function(group){
                return {
                    isGroup:true,
                    collapsed:false,
                    parentId: group[0].parentId,
                    name: group[0].groupName,
                    id: group[0].groupId,
                    url:group[0].url,
                    status: 'Online',
                    cameras:group
                };
            });

            console.log('groups',cams[1]);

            //6 union cameras back
            cameras = _.union(cams[0],cams[1]);

            //7 sort again
            cameras = _.sortBy(cameras,cameraSorter);

            //8. Group by servers
            $scope.cameras = _.groupBy(cameras, function(camera){ return camera.parentId; });

            $scope.allcameras = cameras;

            console.log('Cameras',$scope.cameras);
        },function(){
            alert('network problem');
        });
    }
    mediaserver.getMediaServers().then(function(data){
        console.log('getMediaServers',data.data);

        _.each(data.data,function(server){
            server.url = extractDomain(server.url);
            server.collapsed = server.status !== 'Online' && (server.allowAutoRedundancy || server.flags.indexOf('SF_Edge')<0);
        });
        $scope.mediaServers = data.data;
        getCameras();
    },function(){
        alert('network problem');
    });

    mediaserver.getSettings().then(function (r) {
        $scope.settings = {
            systemName: r.data.reply.systemName,
            port: r.data.reply.port,
            id: r.data.reply.id
        };
    });


});
