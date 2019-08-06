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

    getAllCameras(company): Observable<any> {
        return this.http.get(this.CONFIG.cacamerasUrl, { params: { company }});
    }

    getIntegrations(): Observable<any> {
        return this.http.get(this.CONFIG.apiBase + '/integrations');
    }
}
