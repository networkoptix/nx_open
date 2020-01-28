import { Injectable } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';
import { NxConfigService } from './nx-config';
import { from, of, throwError } from 'rxjs';
import { mergeMap, retryWhen } from 'rxjs/operators';
import { Location } from '@angular/common';


interface User {
    canBeEdited: boolean;
    canBeDeleted: boolean;
    email: string;
    isCloud: boolean;
    isEnabled: boolean;
    userRoleId: string;
    permissions: string;
    // TODO: Remove the trash below after #VMS-2968
    name: string;
    fullName: string;
}


export class NxSystemAPI {
    /*
    * System API is a unified service for making API requests to media servers
    *
    * There are several modes for this service:
    * 1. Upper level: working locally (no systemId) or through the cloud (with systemId)
    * 2. Lower level: working with default server (no serverID) or through the proxy (with serverId)
    *
    * Service supports authentication methods for all these cases
    * 1. working locally we use cookie authentication on server
    * 2. working through cloud we use cloudAPI method to get auth keys
    *
    * Service also supports re-authentication?
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
    private authGet: string;
    private authPost: string;
    private authPlay: string;

    readonly emptyId = '{00000000-0000-0000-0000-000000000000}';

    CONFIG: any;
    http: any;
    location: any;

    abortReason: string;
    serverId: string;
    systemId: string;
    currentUser: any;
    userEmail: string;
    userRequest: any;
    urlBase: string;
    unauthorizedCallback: any;

    constructor(http, config, location, userEmail, systemId, serverId, unauthorizedCallback) {
        this.http = http;
        this.CONFIG = config;
        this.location = location;
        this.init(userEmail, systemId, serverId, unauthorizedCallback);
    }

    private getUrlBase() {
        let urlBase = '';
        if (this.systemId) {
            urlBase = window.location.protocol + '//' +
            (this.CONFIG.trafficRelayHost.replace('{host}', window.location.host).replace('{systemId}', this.systemId));
        }
        urlBase += '/web';
        if (this.serverId) {
            urlBase += '/proxy/' + this.serverId;
        }
        return urlBase;
    }

    private generateGetUrl(url: string, data: any, absUrl?: boolean) {
        let params = new HttpParams();
        Object.keys(data).forEach((key: string) => {
            params = params.set(key, data[key]);
        });
        if (absUrl) {
            const proto = window.location.protocol;
            const hostName = window.location.hostname;
            const usePort = window.location.port;
            const port = usePort ? `:${usePort}` : '';
            url = `${proto}//${hostName}${port}${url}`;
        } else {
            url = `${this.urlBase}${url}`;
        }
        return `${url}${url.indexOf('?') > -1 ? '&' : '?'}${params}`;
    }

    private get(url: string, params?: any) {
        params = params || {};
        if (this.authGet) {
            params.auth = this.authGet;
        }
        const fullUrl = `${this.urlBase}${url}`;
        return this.http.get(fullUrl, {params}).pipe(
            retryWhen((request) => this.retryHandler(request))
        );
    }

    private post(url: string, data?: any) {
        data = data || {};
        const fullUrl = `${this.urlBase}${url}`;
        const params: any = {};
        if (this.authPost) {
            params.auth = this.authPost;
        }
        return this.http.post(fullUrl, data, {params}).pipe(
            retryWhen((request) => this.retryHandler(request))
        );
    }

    private retryHandler(request) {
        return request.pipe(mergeMap((error: any, attempt: number) => {
            if (attempt === 0) {
                if (error.status === 401 || error.status === 403 || error.resultCode === 'forbidden') {
                    return from(this.unauthorizedCallback(error));
                } else if (error.status === 503) { // Repeat the request once again for 503 error
                    return of();
                }
            }
            return throwError(error);
        }));
    }

    init(userEmail, systemId, serverId, unauthorizedCallback) {
        this.setAuthKeys('', '', '');
        this.userEmail = userEmail;
        this.systemId = systemId;
        this.serverId = serverId;
        this.unauthorizedCallback = unauthorizedCallback;
        this.urlBase = this.getUrlBase();
    }

    cleanId(id) {
        return id.replace('{', '').replace('}', '');
    }

    apiHost() {
        if (this.systemId) {
            return this.CONFIG.trafficRelayHost.replace('{host}', window.location.host).replace('{systemId}', this.systemId);
        }
        return window.location.host;
    }

    /* Authentication */
    getCurrentUser (forceReload?: boolean): Promise<any> {
        if (forceReload) { // Clean cache to
            this.currentUser = undefined;
            this.userRequest = undefined;
        }
        if (this.currentUser) { // We have user - return him right away
            return Promise.resolve(this.currentUser);
        }
        if (this.userRequest) { // Currently requesting user
            return this.userRequest;
        }
        if (this.userEmail) { // Cloud portal mode - getCurrentUser is not working
            this.userRequest = this.get('/ec2/getUsers').toPromise()
                .then((result: any) => {
                    this.currentUser = result.find((user) => {
                        return user.name.toLowerCase() === this.userEmail.toLowerCase();
                    });
                    return this.currentUser;
                });
        } else { // Local system mode ???
            this.userRequest = this.get('/api/getCurrentUser').toPromise()
                .then((result) => {
                    this.currentUser = result;
                    return this.currentUser;
                });
        }
        this.userRequest.finally(() => {
            this.userRequest = undefined; // Clear cache in case of errors
        });
        return this.userRequest;
    }

    getRolePermissions(roleId) {
        return this.get('/ec2/getUserRoles', {id: roleId});
    }

    checkPermissions(flag) {
        // TODO: getCurrentUser will not work on portal for 3.0 systems, think of something
        return this.getCurrentUser().then((user: any) => {
            if (!user.isAdmin && this.isEmptyId(user.userRoleId)) {
                return this.getRolePermissions(user.userRoleId).subscribe((role: any) => {
                    return role.permissions.indexOf(flag) > -1;
                });
            }
            return user.isAdmin || user.permissions.indexOf(flag) > -1;
        });
    }

    setAuthKeys(authGet, authPost, authPlay) {
        this.authGet = authGet;
        this.authPost = authPost;
        this.authPlay = authPlay;
    }
    /* End of Authentication  */

    /* Server settings */
    getServerTimes() {
        return this.get('/ec2/getTimeOfServers');
    }

    getSystemTime () {
        return this.get('/api/synchronizedTime');
    }

    updateOrGetSettings(updateParams) {
        return this.get('/api/systemSettings', updateParams);
    }
    /* End of Server settings */

    /* Working with users*/
    getAggregatedUsersData() {
        return this.get('/api/aggregator?exec_cmd=ec2%2FgetUsers&exec_cmd=ec2%2FgetPredefinedRoles&exec_cmd=ec2%2FgetUserRoles');
    }

    saveUser(user) {
        return this.post('/ec2/saveUser', this.cleanUserObject(user));
    }

    deleteUser(userId) {
        return this.post('/ec2/removeUser', {id: userId});
    }

    isEmptyId(id) {
        return !id || id === this.emptyId;
    }

    cleanUserObject(user) { // Remove unnesesary fields from the object
        const cleanedUser: any = {};
        if (user.id) {
            cleanedUser.id = user.id;
        }
        const supportedFields = ['email', 'name', 'fullName', 'userId', 'userRoleId', 'permissions', 'isCloud', 'isEnabled'];
        supportedFields.forEach((field: string) => {
            if (field in user) {
                cleanedUser[field] = user[field];
            }
        });
        if (!cleanedUser.userRoleId) {
            cleanedUser.userRoleId = this.emptyId;
        }
        cleanedUser.email = cleanedUser.email.toLowerCase();
        cleanedUser.name = cleanedUser.name.toLowerCase();
        return cleanedUser;
    }

    userObject(fullName, email): User {
        return {
            canBeEdited : true,
            canBeDeleted : true,
            email,
            isCloud: true,
            isEnabled: true,
            userRoleId: this.emptyId,
            permissions: '',
            // TODO: Remove the trash below after #VMS-2968
            name: email,
            fullName,
        };
    }
    /* End of Working with users */

    /* Cameras and Servers */
    getCameras(id) {
        const params = id ? {id: this.cleanId(id)} : {};
        return this.get('/ec2/getCamerasEx', params);
    }

    getMediaServers(id?) {
        const params = id ? {id: this.cleanId(id)} : {};
        return this.get('/ec2/getMediaServersEx', params);
    }

    getMediaServersAndCameras() {
        return this.get('/api/aggregator?exec_cmd=ec2%2FgetMediaServersEx&exec_cmd=ec2%2FgetCamerasEx');
    }

    getResourceTypes() {
        return this.get('/ec2/getResourceTypes');
    }
    /* End of Cameras and Servers */

    /* Formatting urls */
    previewUrl(cameraId, time?, width?, height?) {
        const data: any = {
            cameraId: this.cleanId(cameraId)
        };
        let endpoint = '/ec2/cameraThumbnail';

        if (time) {
            data.time = time;
        } else {
            endpoint += '?ignoreExternalArchive';
            data.time = 'LATEST';
        }

        if (width) {
            data.width = width;
        }

        if (height) {
            data.height = height;
        }
        if (this.systemId) {
            data.auth = this.authGet;
        }
        return this.generateGetUrl(endpoint, data);
    }

    hlsUrl(cameraId, position, resolution) {
        const data: any = {
            auth: this.authGet
        };
        if (position) {
            data.pos = position;
        }
        const url = `/hls/${this.cleanId(cameraId)}.m3u8?${resolution}`;
        return this.generateGetUrl(url, data, true);
    }

    webmUrl(cameraId, position, resolution, force) {
        const data: any = {
            auth: this.authGet,
            resolution
        };
        if (position) {
            data.pos = position;
        }
        const url = `/media/${this.cleanId(cameraId)}.webm?rt`;
        return this.generateGetUrl(url, data, force);
    }
    /* End of formatting urls */

    /* Working with archive*/
    getRecords(cameraId, startTime, endTime, detail, limit, label, periodsType) {
        const date = new Date();
        if (typeof(startTime) === 'undefined') {
            startTime = date.getTime() - 30 * 24 * 60 * 60 * 1000;
        }
        if (typeof(endTime) === 'undefined') {
            endTime = date.getTime() + 100 * 1000;
        }
        if (typeof(detail) === 'undefined') {
            detail = (endTime - startTime) / 1000;
        }

        if (typeof(periodsType) === 'undefined') {
            periodsType = 0;
        }
        const params: any = {
            cameraId: this.cleanId(cameraId),
            detail,
            endTime,
            periodsType,
            startTime
        };
        if (limit) {
            params.limit = limit;
        }
        // RecordedTimePeriods
        return this.get(`/ec2/recordedTimePeriods?flat&keepSmallChunks&${label || ''}`, params);
    }
    /* End of Working with archive*/

    setCameraPath(cameraId) {
        let systemLink = '';
        const route = this.location.path().indexOf('/embed') === 0 ? '/embed/' : '';

        if (this.systemId) {
            if (route !== '') {
                systemLink = route + this.systemId;
            } else {
                systemLink = `/systems/${this.systemId}`;
            }
        }
        this.location.path(`${systemLink}/view/${this.cleanId(cameraId)}`, false);
    }

    /* Health Monitor */
    getHealthManifest() {
        return this.get('/ec2/metrics/manifest');
    }

    getHealthValues() {
        return this.get('/ec2/metrics/values');
        // return this.http.get('/getdata');
    }
    getHealthAlarms() {
        return this.get('/ec2/metrics/alarms');
    }

    getAggregateHealthReport() {
        return this.get('/api/aggregator?exec_cmd=ec2%2Fmetrics%2Fmanifest&exec_cmd=ec2%2Fmetrics%2Fvalues&exec_cmd=ec2%2Fmetrics%2Falarms');
    }
}


@Injectable({
    providedIn: 'root'
})
export class NxSystemAPIService {
    CONFIG: any;
    location: any;
    systemConnections: { [key: string]: NxSystemAPI };

    constructor(private http: HttpClient,
                private config: NxConfigService,
                location: Location) {
        this.location = location;
        this.CONFIG = this.config.getConfig();
        this.systemConnections = {};
    }

    createConnection(user, systemId, serverId, unauthorizedCallback) {
        if (systemId in this.systemConnections) {
            return this.systemConnections[systemId];
        } else if (serverId in this.systemConnections) {
            return this.systemConnections[serverId];
        }
        return new NxSystemAPI(this.http, this.CONFIG, this.location, user, systemId, serverId, unauthorizedCallback);
    }
}
