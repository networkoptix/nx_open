'use strict';

angular.module('nxCommon')
    .factory('systemAPI', ['$http', '$q', '$location', '$rootScope', '$log',
    function ($http, $q, $location, $rootScope, $log) {

        /*
        * System API is a unified service for making API requests to media servers
        *
        * There are several modes for this service:
        * 1. Upper level: working locally (no systemId) or through the cloud (with systemId)
        * 2. Lower level: workgin with default server (no serverID) or through the proxy (with serverId)
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

        
        function ServerConnection(userEmail, systemId, serverId, unauthorizedCallBack){
            // If we have systemId - this is a cloud connection
            // if we have serverId - we ought to use proxy
            this.userEmail = userEmail;
            this.systemId = systemId;
            this.serverId = serverId;
            this.unauthorizedCallBack = unauthorizedCallBack;
            this.urlBase = this._getUrlBase();
        }

        // Connections cache
        var connections = {};
        function connect (userEmail, systemId, serverId, unauthorizedCallBack){
            var key = systemId + '_' + serverId;
            if(!connections[key]){
                connections[key] = new ServerConnection(userEmail, systemId, serverId, unauthorizedCallBack);
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
        ServerConnection.prototype._wrapRequest = function(method, url, data, repeat){
            var self = this;
            var auth = method=='GET'? this.authGet() : this.authPost();
            var getData = method=='GET'? data : null;
            var postData = method=='POST'? data : null;
            var requestUrl = this._setGetParams(url, getData, this.systemId && auth);

            var canceler = $q.defer();
            var request = $http({
                method: method,
                url: requestUrl,
                data: postData,
                config: {
                    timeout: canceler.promise
                }
            });

            var promise = request.catch(function(error){
                if(error.status == 401 || error.status == 403  || error.data && error.data.resultCode == "forbidden"){
                    if(!repeat && self.unauthorizedCallBack){ // first attempt
                        // Here we call a handler for unauthorised request. If handler promises success - we repeat the request once again.
                        // Handler is supposed to try and update auth keys
                        return self.unauthorizedCallBack(error).then(function(){
                            return self._wrapRequest(method, url, data, true);
                        },function(){
                            $rootScope.$broadcast("unauthirosed_" + self.systemId);
                            return $q.reject(error);
                        });
                    }
                    // Not authorised request - we lost connection to the system, broadcast this for active controller to handle the situation if needed
                    $rootScope.$broadcast("unauthirosed_" + self.systemId);
                }
                if(!repeat && error.status == 503){ // Repeat the request once again for 503 error
                    return self._wrapRequest(method, url, data, true);
                }

                return $q.reject(error);// We cannot handle the problem at this level, pass it up
            });

            promise.then(function(){
                canceler = null;
            },function(){
                canceler = null;
            });
            promise.abort = function(reason){
                if(canceler) {
                    canceler.resolve('abort request: ' + reason);
                }
            };

            return promise;
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
            return this._wrapRequest('GET', url, data);
        };
        ServerConnection.prototype._post = function(url, data){
            return this._wrapRequest('POST', url, data);
        };
        /* End of helpers */


        /* Authentication */
        ServerConnection.prototype.getCurrentUser = function (forcereload){
            if(forcereload){ // Clean cache to 
                self.currentUser = null;
                self.userRequest = null;
            }
            if(this.currentUser){ // We have user - return him right away
                return $q.resolve(this.currentUser);
            }
            if(this.userRequest){ // Currently requesting user
                return this.userRequest;
            }
            
            var self = this;
            if(self.userEmail){ // Cloud portal mode - getCurrentUser is not working
                this.userRequest = this._get('/ec2/getUsers').then(function(result){
                    self.currentUser = _.find(result.data, function(user){
                        return user.name.toLowerCase() == self.userEmail.toLowerCase();
                    });
                    return self.currentUser;
                });
            }else{ // Local system mode ???
                this.userRequest = this._get('/api/getCurrentUser').then(function(result){
                    self.currentUser = result.data.reply;
                    return self.currentUser;
                });
            }

            this.userRequest.finally(function(){
                self.userRequest = null; // Clear cache in case of errors
            });
            return this.userRequest;
        };

        ServerConnection.prototype.checkPermissions = function(flag){
            // TODO: getCurrentUser will not work on portal for 3.0 systems, think of something
            return this.getCurrentUser().then(function(user) {
                return user.isAdmin ||
                       user.permissions.indexOf(flag)>=0;
            });
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
            var defaultConnection = connect(null, null, serverId, function(error){
                // Unauthorised request here

                $log.error("reload on lost connection",error);
                window.location.reload(); // just reload the page and it supposed to handle the problem
                return $q.reject(); // Do not repeat last request
            });
            return defaultConnection;
        }

        return connect;
    }]);
