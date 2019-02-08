import { Injectable, OnDestroy }        from '@angular/core';
import { BehaviorSubject, Observable }  from 'rxjs';
import { NxCloudApiService }            from '../../services/nx-cloud-api';
import { NxConfigService }              from '../../services/nx-config';


interface Platform {
    file: string;
    name: string;
    order: string;
    url: string;
}

@Injectable({
    providedIn: 'root'
})
export class IntegrationService implements OnDestroy {

    plugins: any;
    pluginsSubject = new BehaviorSubject([]);
    selectedPluginSubject = new BehaviorSubject(undefined);
    inReview: boolean;

    constructor(private api: NxCloudApiService,
                private config: NxConfigService) {
        this.getIntegrations().subscribe(result => {
            this.plugins = result.data;
            this.formatPlugins();
            this.pluginsSubject.next(this.plugins);
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

    private formatPlugins() {
        if (!this.plugins) {
            return;
        }

        this.inReview = false;

        this.plugins.forEach((plugin) => {
            if (!this.inReview && plugin.pending) {
                this.inReview = true;
            }

            if (plugin.downloadFiles) {
                const downloadPlatforms = plugin.downloadFiles;
                plugin.downloadFiles = [];

                for (const platformName in downloadPlatforms) {
                    // If there is no file url or its the name for an additional field skip
                    if (typeof downloadPlatforms[platformName] !== 'string' ||
                        !downloadPlatforms[platformName] ||
                        platformName.match(/-file-[\d]+-name/)) {

                        continue;
                    }

                    const platform: Platform = { file: '', name: '', order: '', url: '' };
                    // If the platformName is additional file we replace it with the correct name
                    if (platformName.match(/-file-[\d]+/)) {
                        platform.name = downloadPlatforms[`${platformName}-name`];
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
        });
    }

    getPluginBy(id) {
        if (this.plugins) {
            this.selectedPluginSubject.next(this.plugins.find(plugin => plugin.id === Number(id)));
        }

        return undefined;
    }

    ngOnDestroy() {
        this.pluginsSubject.unsubscribe();
    }
}
