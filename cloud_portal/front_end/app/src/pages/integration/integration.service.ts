import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class IntegrationService {

    constructor(private http: HttpClient) {
    }

    // getIntegrations(): Observable<any> {
    //     return this.http
    //         .get([URL]);
    // }

    getMockData(): Observable<any> {
        return this.http.get('src/pages/integration/integrations.json');
    }
}
