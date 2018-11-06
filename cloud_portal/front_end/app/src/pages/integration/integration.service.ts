import { Injectable, OnDestroy }        from '@angular/core';
import { HttpClient }                   from '@angular/common/http';
import { BehaviorSubject, Observable }  from 'rxjs';
import { NxCloudApiService }            from '../../services/nx-cloud-api';


interface Platform {
    file: string;
    name: string;
    url: string;
}

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
            this.formatDownloads();
            this.pluginsSubject.next(this.plugins);
        });
    }

    private getIntegrations(): Observable<any> {
        return this.api.getIntegrations();
    }

    private formatDownloads() {
        this.plugins.forEach((plugin) => {
            if (plugin.downloadFiles) {
                let downloadPlatforms = plugin.downloadFiles;
                plugin.downloadFiles = [];
                Object.keys(downloadPlatforms).forEach((platformName) => {
                    if(!downloadPlatforms[platformName] || platformName.match(/additional-file-[\d]+-name/)) {
                        return;
                    }
                    let platform: Platform = { file: '', name: '', url: '' };
                    platform.name = platformName.replace('-file', '')
                            .replace('-', ' ');
                    platform.name = platform.name[0].toUpperCase() + platform.name.slice(1);
                    if (platformName.match(/additional-file-[\d]+/)) {
                        platform.name = downloadPlatforms[`${platformName}-name`]
                    }
                    platform.url = downloadPlatforms[platformName];
                    platform.file = platform.url.slice(platform.url.lastIndexOf('/') + 1);
                    plugin.downloadFiles.push(platform);
                });
                plugin.downloadFiles = plugin.downloadFiles.sort((a, b) => {
                    if (a.name < b.name) {
                        return -1;
                    } else if (a.name > b.name) {
                        return 1;
                    }
                    return 0;
                });
            }
        });
    }

    getPluginBy(id) {
        if (this.plugins) {
            return this.plugins.find(plugin => plugin.id === Number(id));
        }

        return undefined;
    }

    ngOnDestroy() {
        this.pluginsSubject.unsubscribe();
    }
}
