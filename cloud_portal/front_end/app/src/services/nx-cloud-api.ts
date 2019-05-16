import { Injectable }      from '@angular/core';
import { HttpClient }      from '@angular/common/http';
import { Observable }      from 'rxjs';
import { NxConfigService } from './nx-config';

@Injectable({
    providedIn: 'root'
})
export class NxCloudApiService {
    CONFIG: any;

    constructor(private http: HttpClient,
                private config: NxConfigService) {
        this.CONFIG = config.getConfig();
    }

    getIntegrations(): Observable<any> {
        return this.http.get(this.CONFIG.apiBase + '/integrations');
    }

    getIntegrationBy(params: any = {}): Observable<any> {
        let uri = this.CONFIG.apiBase + '/integration/' + params.plugin;
        uri += (params.status) ? '/' + params.status : '' ;

        return this.http.get(uri);
    }


    getIPVD(): Observable<any> {
        return this.http.get(this.CONFIG.apiBase + '/ipvd');
    }

    reloadIPVD(): Observable<any> {
        return this.http.post(this.CONFIG.apiBase + '/ipvd', {});
    }
}
