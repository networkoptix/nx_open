import { Inject, Injectable } from '@angular/core';
import { HttpClient }         from '@angular/common/http';
import { Observable }         from 'rxjs';
import { NxCloudApiService }  from '../../services/nx-cloud-api';

@Injectable({
    providedIn: 'root'
})
export class IntegrationService {

    constructor(private http: HttpClient,
                private api: NxCloudApiService) {
    }

    getIntegrations(): Observable<any> {
        return this.api.getIntegrations();
    }

    getMockData(): Observable<any> {
        return this.http.get('src/pages/integration/integrations.json');
    }
}
