import { Injectable, OnDestroy }        from '@angular/core';
import { HttpClient }                   from '@angular/common/http';
import { BehaviorSubject, Observable }  from 'rxjs';
import { NxCloudApiService }            from '../../services/nx-cloud-api';

@Injectable({
    providedIn: 'root'
})
export class IntegrationService implements OnDestroy {

    plugins: any;
    pluginsSubject = new BehaviorSubject([]);

    constructor(private http: HttpClient,
                private api: NxCloudApiService) {

        this.getIntegrations().subscribe(result => {
            this.plugins = result.data;
            this.pluginsSubject.next(this.plugins);
        });
    }

    private getIntegrations(): Observable<any> {
        return this.api.getIntegrations();
    }

    getPluginBy(id) {
        if (this.plugins) {
            return this.plugins.find(plugin => plugin.id === Number(id));
        }

        return {};
    }

    ngOnDestroy() {
        this.pluginsSubject.unsubscribe();
    }
}
