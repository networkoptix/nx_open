import { Injectable, OnDestroy }       from '@angular/core';
import { BehaviorSubject, Observable } from 'rxjs';
import { NxCloudApiService }           from '../../services/nx-cloud-api';
import { NxConfigService }             from '../../services/nx-config';

interface Platform {
    file: string;
    name: string;
    order: string;
    url: string;
    noFollow: boolean;
}

@Injectable({
    providedIn: 'root'
})
export class IntegrationService implements OnDestroy {
    pluginsSubject = new BehaviorSubject([]);
    inReview: boolean;

    constructor(private api: NxCloudApiService,
                private config: NxConfigService) {

        this.getIntegrations()
            .subscribe(result => {
                this.pluginsSubject.next(result.data);
            });
    }

    private getIntegrations(): Observable<any> {
        return this.api.getIntegrations();
    }

    private setScreenshots(section) {
        if (section) {
            section.screenshots = Object.keys(section).filter((element) => {
                return element.match(/screenshot/i) && section[element];
            }).sort().map((key) => section[key]);
            if (section.screenshots.length < 1) {
                delete section.screenshots;
            }
        }
    }

    format(plugin) {
        if (plugin.downloadFiles) {
            const downloadPlatforms = plugin.downloadFiles;
            plugin.downloadFiles = [];

            for (const platformName in downloadPlatforms) {
                // If there is no file url or its the name for an additional field skip
                if (typeof downloadPlatforms[platformName] !== 'string' ||
                    !downloadPlatforms[platformName] ||
                    platformName.match(/-file-[\d]+-name/) ||
                    platformName.match(/external-link-name/)) {
                    continue;
                }

                const platform: Platform = { file: '', name: '', order: '', url: '', noFollow: false };
                // If the platformName is additional file we replace it with the correct name
                if (platformName.match(/-file-[\d]+/) || platformName.match(/external-link/)) {
                    platform.name = downloadPlatforms[`${platformName}-name`];
                    if (platformName.match(/external-link/)) {
                        platform.noFollow = true;
                    }
                } else {
                    platform.name = this.config.config.defaultPlatformNames[platformName];
                }

                platform.url = downloadPlatforms[platformName];
                platform.file = platform.url.slice(platform.url.lastIndexOf('/') + 1);
                platform.order = plugin.downloadFilesOrder[platformName];
                if (!platform.file) {
                    platform.file = platform.url;
                }
                plugin.downloadFiles.push(platform);
            }
            // sort by name and then sort by file name.
            plugin.downloadFiles = plugin.downloadFiles.sort((a, b) => {
                if (a.order < b.order) {
                    return -1;
                }  else if (a.order > b.order) {
                    return 1;
                }
                return 0;
            });
        }

        this.setScreenshots(plugin.instructions);
        this.setScreenshots(plugin.overview);

        return plugin;
    }

    getIntegrationBy(id: number, status: string): Observable<any> {
        return this.api.getIntegrationBy(id, status);
    }

    ngOnDestroy() {
        this.pluginsSubject.unsubscribe();
    }
}
