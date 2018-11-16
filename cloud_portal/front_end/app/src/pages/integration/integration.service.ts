import { Injectable, OnDestroy }        from '@angular/core';
import { HttpClient }                   from '@angular/common/http';
import { BehaviorSubject, Observable }  from 'rxjs';
import { NxCloudApiService }            from '../../services/nx-cloud-api';
import { NxConfigService }              from "../../services/nx-config";


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
                private api: NxCloudApiService,
                private config: NxConfigService) {

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
        if (!this.plugins) {
            return;
        }
        this.plugins.forEach((plugin) => {
            if (plugin.downloadFiles) {
                const downloadPlatforms = plugin.downloadFiles;
                plugin.downloadFiles = [];

                for (const platformName in downloadPlatforms) {
                    // If there is no file url or its the name for an additional field skip
                    if (typeof downloadPlatforms[platformName] !== 'string' ||
                        !downloadPlatforms[platformName] ||
                        platformName.match(/additional-file-[\d]+-name/)) {

                        continue;
                    }

                    const platform: Platform = { file: '', name: '', url: '' };
                    // If the platformName is additional file we replace it with the correct name
                    if (platformName.match(/additional-file-[\d]+/)) {
                        platform.name = downloadPlatforms[`${platformName}-name`];
                    } else {
                        platform.name = this.config.config.defaultPlatformNames[platformName];
                    }

                    platform.url = downloadPlatforms[platformName];
                    platform.file = platform.url.slice(platform.url.lastIndexOf('/') + 1);
                    plugin.downloadFiles.push(platform);
                }
                // sort by name and then sort by file name.
                plugin.downloadFiles = plugin.downloadFiles.sort((a, b) => {
                    // '~~~' are used to weight downloads
                    const weight = '~~~';
                    if (a.name + weight < b.name + weight) {
                        return -1;
                    } else if (a.name + weight > b.name + weight) {
                        return 1;
                    }
                    if (a.file + weight < b.file + weight) {
                        return - 1;
                    } else if (a.file + weight > b.file + weight) {
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
