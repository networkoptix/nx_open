'use strict';

angular.module('nxCommon')
    .factory('systemAPI', ['$http', '$q', '$location', function ($http, $q, $location) {

        /*
        * System API is a unified service for making API requests to media servers
        *
        * There are several modes for this service:
        * 1. Upper level: working locally (no systemId) or through the cloud (with systemId)
        * 2. Lower level: workgin with default server (no serverID) or through the proxy (with serverId)
        * TODO: get rid of working through cloud portal API
        *
        * Service supports authentification methods for all these cases
        * 1. working locally we use cookie authentification on server
        * 2. working through cloud we use cloudAPI method to get auth keys
        *
        * Service also supports re-authentification?
        *
        *
        * Service also should support global handlers for responses:
        * 1. Not authorised
        * 2. Server offline
        * 3. Server not available
        *
        * Other error handling is done outside. For example, in process service, or in model
        * No http cache here - caching is handled either by browser or by upper-level model
        *
        * Service is initialised to work with specific system and server.
        * Each instance representing a single connection and is cached
        *
        * 
        * TODO (v 3.2): Support websocket connection to server as well
        * */

        
        function ServerConnection(systemId, serverId){
            // If we have systemId - this is a cloud connection
            // if we have serverId - we ought to use proxy
            
            this.systemId = systemId;
            this.serverId = serverId;
            this.urlBase = this._getUrlBase();
        }

        // Connections cache
        var connections = {};
        function connect (systemId, serverId){
            var key = systemId + '_' + serverId;
            if(!connections[key]){
                connections[key] = new ServerConnection(systemId, serverId);
            }
            return connections[key];
        }
        ServerConnection.prototype.connect = connect;


        /* Helpers */
        ServerConnection.prototype._getUrlBase = function(){
            var urlBase = '';
            if(this.systemId){
                urlBase += Config.gatewayUrl + '/' + this.systemId;
            }
            urlBase += '/web';
            if(this.serverId){
                urlBase += '/proxy/' + this.serverId;
            }
            return urlBase;
        };
        ServerConnection.prototype._wrapRequest = function(method, url, data, config, repeat){
            var self = this;
            // TODO: handle common situations here - like offline or logout
            // Raise a global event in both cases. Let outer code handle this global request
            var promise = $http({
                method: method,
                url: url,
                data: data,
                config: config
            });
            return promise.catch(function(error){
                if(error.status == 503 && !repeat){ // Repeat the request once again for 503 error
                    return self._wrapRequest(method, url, data, config, true);
                }
                return $q.reject(error);
            });
        };
        ServerConnection.prototype._setGetParams = function(url, data, auth){
            if(auth){
                data = data || {}
                data.auth = auth;
            }
            if(data){
                url += (url.indexOf('?')>0)?'&':'?';
                url += $.param(data);
            }
            return this.urlBase + url;
        };
        ServerConnection.prototype._get = function(url, data){
            url = this._setGetParams(url, data, this.systemId && this.authGet());
            var canceller = $q.defer();
            var obj = this._wrapRequest('GET', url, null, { timeout: canceller.promise });
            obj.then(function(){
                canceller = null;
            },function(){
                canceller = null;
            });
            obj.abort = function(reason){
                if(canceller) {
                    canceller.resolve('abort request: ' + reason);
                }
            };
            return obj;
        };
        ServerConnection.prototype._post = function(url, data){
            url = this._setGetParams(url, null, this.systemId && this.authPost());
            return this._wrapRequest('POST', url, data);
        };
        /* End of helpers */


        /* Authentication */
        ServerConnection.prototype.getCurrentUser = function (forcereload){
            if(!this.cacheCurrentUser || forcereload){
                this.cacheCurrentUser = this._get('/api/getCurrentUser');
            }
            return this.cacheCurrentUser;
        };

        ServerConnection.prototype.setAuthKeys = function(authGet, authPost, authPlay){
            this._authGet = authGet;
            this._authPost = authPost;
            this._authPlay = authPlay;
        };
        ServerConnection.prototype.authGet = function(){
            return this._authGet ;
        };
        ServerConnection.prototype.authPost = function(){
            return this._authPost ;
        };
        ServerConnection.prototype.authPlay = function(){
            return this._authPlay; // auth_rtsp
        };
        /* End of Authentication  */

        /* Server settings */
        ServerConnection.prototype.getTime = function(){
            return this._get('/api/gettime?local');
        };
        /* End of Server settings */

        /* Working with users*/
        ServerConnection.prototype.getAggregatedUsersData = function(){
            return this._get('/api/aggregator?exec_cmd=ec2%2FgetUsers&exec_cmd=ec2%2FgetPredefinedRoles&exec_cmd=ec2%2FgetUserRoles');
        };
        ServerConnection.prototype.saveUser = function(user){
            return this._post('/ec2/saveUser', this.cleanUserObject(user));
        };
        ServerConnection.prototype.deleteUser = function(userId){
            return this._post('/ec2/removeUser', {id:userId});
        }
        ServerConnection.prototype.cleanUserObject = function(user){ // Remove unnesesary fields from the object
            var cleanedUser = {};
            if(user.id){
                cleanedUser.id = user.id;
            }
            var supportedFields = ['email', 'name', 'fullName', 'userId', 'userRoleId', 'permissions', 'isCloud', 'isEnabled'];

            for(var i in supportedFields){
                cleanedUser[supportedFields[i]] = user[supportedFields[i]];
            }
            if(!cleanedUser.userRoleId){
                cleanedUser.userRoleId = '{00000000-0000-0000-0000-000000000000}';
            }
            cleanedUser.email = cleanedUser.email.toLowerCase();
            cleanedUser.name = cleanedUser.name.toLowerCase();
            return cleanedUser;
        };
        ServerConnection.prototype.userObject = function(fullName, email){
            return {
                'canBeEdited' : true,
                'canBeDeleted' : true,

                'email': email,

                'isCloud': true,
                'isEnabled': true,

                'userRoleId': '{00000000-0000-0000-0000-000000000000}',
                'permissions': '',

                //TODO: Remove the trash below after #VMS-2968
                'name': email,
                'fullName': fullName,
            }
        };
        /* End of Working with users */

        /* Cameras and Servers */
        ServerConnection.prototype._cleanId = function(id) {
            return id.replace('{','').replace('}','');
        };
        ServerConnection.prototype.getCameras = function(id){
            var params = id?{id:this._cleanId(id)}:null;
            return this._get('/ec2/getCamerasEx',params);
        };
        ServerConnection.prototype.getMediaServers = function(id){
            var params = id?{id:this._cleanId(id)}:null;
            return this._get('/ec2/getMediaServersEx',params);
        };
        ServerConnection.prototype.getMediaServersAndCameras = function(){
            return this._get('/api/aggregator?exec_cmd=ec2%2FgetMediaServersEx&exec_cmd=ec2%2FgetCamerasEx');
        }
        ServerConnection.prototype.getResourceTypes = function(){
            return this._get('/ec2/getResourceTypes');
        };
        /* End of Cameras and Servers */

        /* Formatting urls */
        ServerConnection.prototype.previewUrl = function(cameraPhysicalId, time, width, height){
            var data = {
                    physicalId:cameraPhysicalId,
                    time:time  || 'LATEST'
                };

            if(width){
                data.width = width;
            }
            if(height){
                data.height = height;
            }
            return this._setGetParams('/api/image', data, this.systemId && this.authGet());
        };
        ServerConnection.prototype.hlsUrl = function(cameraId, position, resolution){
            var data = {};
            if(position){
                data.pos = position;
            }
            return this._setGetParams('/hls/' + cameraId + '.m3u8?' + resolution, data, this.authGet());
        };
        ServerConnection.prototype.webmUrl = function(cameraId, position, resolution){
            var data = {
                resolution:resolution
            };
            if(position){
                data.pos = position;
            }
            return this._setGetParams('/media/' + cameraId + '.webm?rt' , data, this.authGet());
        };
        /* End of formatting urls */

        /* Working with archive*/
        ServerConnection.prototype.getRecords = function(physicalId, startTime, endTime, detail, limit, label, periodsType){
            //console.log('getRecords',physicalId,startTime,endTime,detail,periodsType);
            var d = new Date();
            if(typeof(startTime)==='undefined'){
                startTime = d.getTime() - 30*24*60*60*1000;
            }
            if(typeof(endTime)==='undefined'){
                endTime = d.getTime() + 100*1000;
            }
            if(typeof(detail)==='undefined'){
                detail = (endTime - startTime) / 1000;
            }

            if(typeof(periodsType)==='undefined'){
                periodsType = 0;
            }

            //RecordedTimePeriods
            return  this._get('/ec2/recordedTimePeriods' +
            '?' + (label||'') +
            (limit?'&limit=' + limit:'') +
            '&physicalId=' + physicalId +
            '&startTime=' + startTime +
            '&endTime=' + endTime +
            '&detail=' + detail +
            '&periodsType=' + periodsType +
            '&flat&keepSmallChunks');
            //http://10.1.5.129:7001/web/ec2/recordedTimePeriods?1h&            physicalId=urn_uuid_32306235-3032-6666-3238-000df120b502&startTime=1488920406724&endTime=1494626406724&detail=3600000&periodsType=0&flat&keepSmallChunks

            //http://localhost:10000/web/ec2/recordedTimePeriods?&limit=100&physicalId=urn_uuid_32306235-3032-6666-3238-000df120b502&startTime=1494625266744&endTime=1494625366744&detail=1      &periodsType=0&flat&keepSmallChunks
        };
        /* End of Working with archive*/

        ServerConnection.prototype.setCameraPath = function(cameraId){
            var systemLink = '';
            if(this.systemId){
                systemLink = '/systems/' + this.systemId;
            }
            $location.path(systemLink + '/view/' + cameraId, false);
        };

        if(Config.webadminSystemApiCompatibility){
            // This is a hack to avoid changing all webadmin controllers at the same time - we initialize default connection
            var serverId = null;
            if(location.search.indexOf('proxy=')>0){
                var params = location.search.replace('?','').split('&');
                for (var i=0;i<params.length;i++) {
                    var pair = params[i].split('=');
                    if(pair[0] === 'proxy'){
                        serverId = pair[1];
                        break;
                    }
                }
            }
            var defaultConnection = connect(null, serverId);
            return defaultConnection;
        }

        return connect;
    }]);
