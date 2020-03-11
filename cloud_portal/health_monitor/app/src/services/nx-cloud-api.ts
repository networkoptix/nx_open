import { Injectable }      from '@angular/core';
import { HttpClient }      from '@angular/common/http';
import { Observable }      from 'rxjs';
import { NxConfigService } from './nx-config';
import {toPromise} from "rxjs-compat/operator/toPromise";

@Injectable({
    providedIn: 'root'
})
export class NxCloudApiService {
    CONFIG: any;

    constructor(private http: HttpClient,
                private config: NxConfigService) {
        this.CONFIG = config.getConfig();
    }

    checkResponseHasError(data: any) {
        if (data && data.resultCode && data.resultCode !== this.CONFIG.responseOk) {
            return data;
        }
        return false;
    }

    disconnect(systemId, password) {
        return this.http.post(this.CONFIG.apiBase + '/systems/disconnect', {
            system_id: systemId,
            password
        });
    }

    getCommonPasswords(): Observable<any> {
        return this.http.get('/static/scripts/commonPasswordsList.json');
    }

    getIntegrations(): Observable<any> {
        return this.http.get(this.CONFIG.apiBase + '/integrations');
    }

    getIntegrationBy(id: number, status: string): Observable<any> {
        let uri = this.CONFIG.apiBase + '/integration/' + id;
        uri += (status) ? '?' + status : '' ;

        return this.http.get(uri);
    }

    getIPVD(): Observable<any> {
        return this.http.get(this.CONFIG.apiBase + '/ipvd');
    }

    getSystemAuth(systemId) {
        return this.http.get(`${this.CONFIG.apiBase}/systems/${systemId}/auth`);
    }

    merge(masterSystemId, slaveSystemId, password) {
        return this.http.post(`${this.CONFIG.apiBase}/systems/merge`, {
            master_system_id: masterSystemId,
            slave_system_id: slaveSystemId,
            password
        }).toPromise();
    }

    notification_send(userEmail, type, message) {
        return this.http.post(`${this.CONFIG.apiBase.replace('/api', '/notifications')}/send`, {
            user_email: userEmail,
            type,
            message
        }).toPromise();
    }

    reloadIPVD(): Observable<any> {
        return this.http.post(this.CONFIG.apiBase + '/ipvd', {});
    }

    registerUser(email, password, firstName, lastName, subscribe, code) {
        return this.http
                   .post(this.CONFIG.apiBase + '/account/register',
                           {
                               email, password,
                               first_name: firstName,
                               last_name : lastName,
                               subscribe, code
                           })
                   .toPromise();
    }

    reactivateUser(userEmail) {
        return this.http.post(this.CONFIG.apiBase + '/account/activate',
                { user_email: userEmail }).toPromise();
    }

    renameSystem(systemId, systemName) {
        return this.http.post(this.CONFIG.apiBase + '/systems/' + systemId + '/name', {
            name: systemName
        }).toPromise().then((result) => {
            this.systems('clearCache');
            return result;
        });
    }

    sendMessage(type, asset, message, userName?, userEmail?) {
        return this.http.post(this.CONFIG.apiBase + '/feedback', {
            message, asset, type, userName, userEmail
        });
    }

    systems (systemId?: string): Observable<any> {
        if (systemId) {
            return this.http.get(this.CONFIG.apiBase + '/systems/' + systemId);
        }
        return this.http.get(this.CONFIG.apiBase + '/systems');
    }

    users(systemId) {
        return this.http.get(`${this.CONFIG.apiBase}/systems/${systemId}/users`);
    }

    unshare(systemId, userEmail) {
        return this.http.post(this.CONFIG.apiBase + '/systems/' + systemId + '/users', {
            user_email: userEmail,
            role: this.CONFIG.accessRoles.unshare
        });
    }

    authKey() {
        return this.http.post(this.CONFIG.apiBase + '/account/authKey', {}).toPromise();
    }

    visitedKey(key) {
        return this.http.get(this.CONFIG.apiBase + '/utils/visitedKey/?key=' + encodeURIComponent(key)).toPromise();
    }

    checkCode(code) {
        return this.http.post(this.CONFIG.apiBase + '/account/checkCode', { code }).toPromise();
    }

    checkAuthCode(code) {
        return this.http.post(this.CONFIG.apiBase + '/account/checkAuthCode', { code }).toPromise();
    }

    login(email, password, remember) {
        // clearCache();
        return this.http.post(this.CONFIG.apiBase + '/account/login', {
            email,
            password,
            remember,
            timezone: Intl && Intl.DateTimeFormat().resolvedOptions().timeZone || ''
        }).toPromise();
    }

    logout() {
        // clearCache();
        return this.http.post(this.CONFIG.apiBase + '/account/logout', {}).toPromise();
    }

    account() {
        return this.http.get(this.CONFIG.apiBase + '/account').toPromise();
    }

    getLanguages() {
        return this.http.get('/static/languages.json').toPromise();
    }

    changeLanguage(language) {
        return this.http.post(this.CONFIG.apiBase + '/utils/language/', {
            language
        }).toPromise();
    }

    getDownloads() {
        return this.http.get(this.CONFIG.apiBase + '/utils/downloads').toPromise();
    }

    getDownloadsHistory(build) {
        return this.http.get(this.CONFIG.apiBase + '/utils/downloads/' + (build || 'history')).toPromise();
    }

    accountPost(account) {
        // strip unnecessary account info
        const accountInfo = {
            email       : account.email,
            first_name  : account.first_name,
            last_name   : account.last_name,
            is_staff    : account.is_staff,
            is_superuser: account.is_superuser || false,
            language    : account.language,
            permissions : account.permissions
        };
        return this.http.post(this.CONFIG.apiBase + '/account', accountInfo).toPromise();
    }

    changePassword(newPassword, oldPassword) {
        return this.http.post(this.CONFIG.apiBase + '/account/changePassword', {
            new_password: newPassword,
            old_password: oldPassword
        }).toPromise();
    }

    reactivate(userEmail) {
        return this.http.post(this.CONFIG.apiBase + '/account/activate', {
            user_email: userEmail
        }).toPromise();
    }

    activate(code) {
        return this.http.post(this.CONFIG.apiBase + '/account/activate', {
            code
        }).toPromise();
    }

    restorePasswordRequest(userEmail) {
        return this.http.post(this.CONFIG.apiBase + '/account/restorePassword', {
            user_email: userEmail
        }).toPromise();
    }

    restorePassword(code, newPassword) {
        return this.http.post(this.CONFIG.apiBase + '/account/restorePassword', {
            code,
            new_password: newPassword
        }).toPromise();
    }

    acceptAgreement(reviewId) {
        return this.http.post(this.CONFIG.apiBase + '/accept_agreement', {
            review_id: reviewId
        }).toPromise();
    }
}
