import { Injectable }      from '@angular/core';
import { HttpClient }      from '@angular/common/http';
import { Observable }      from 'rxjs';
import { NxConfigService } from './nx-config';

@Injectable({
    providedIn: 'root'
})
export class NxCloudApiService {

    constructor(private http: HttpClient,
                private config: NxConfigService) {
    }

    getIntegrations(): Observable<any> {
        return this.http.get(this.config.getConfig().apiBase + '/integrations/');
    }
}
