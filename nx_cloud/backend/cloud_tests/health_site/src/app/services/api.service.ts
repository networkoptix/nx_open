import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable } from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class ApiService {

    constructor(private http: HttpClient) {
    }

    getHealthStatus(): Observable<any> {
        return this.http
                   .get('https://api.status.nxvms.com/health');
    }

    getJSON(): Observable<any> {
        return this.http.get('../assets/layout/tiles.json');
    }
}
