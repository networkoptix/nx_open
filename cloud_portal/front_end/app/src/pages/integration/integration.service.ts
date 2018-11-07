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

    private formatPlatformName(platform: string){
        // Converts 'windows-x64-file' -> 'windows x64'
        platform = platform.replace('-file', '').replace('-', ' ');
        // Makes the first character of the string uppercase
        return platform[0].toUpperCase() + platform.slice(1);
    }

    private formatDownloads() {
        this.plugins.forEach((plugin) => {
            if (plugin.downloadFiles) {
                let downloadPlatforms = plugin.downloadFiles;
                plugin.downloadFiles = [];
                for (let platformName in downloadPlatforms) {
                    // If there is no file url or its the name for an additional field skip
                    if(!downloadPlatforms[platformName] || platformName.match(/additional-file-[\d]+-name/)) {
                        continue
                    }

                    let platform: Platform = { file: '', name: '', url: '' };
                    // If the platformName is additional file we replace it with the correct name
                    if (platformName.match(/additional-file-[\d]+/)) {
                        platform.name = downloadPlatforms[`${platformName}-name`];
                    } else {
                        platform.name = this.formatPlatformName(platformName);
                    }
                    platform.url = downloadPlatforms[platformName];
                    platform.file = platform.url.slice(platform.url.lastIndexOf('/') + 1);
                    plugin.downloadFiles.push(platform);
                }
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
