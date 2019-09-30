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
    config: any = {};
    pluginsSubject = new BehaviorSubject(undefined);
    selectedSectionSubject = new BehaviorSubject([]);
    plugin: any = {};
    inReview: boolean;

    constructor(private api: NxCloudApiService,
                private configService: NxConfigService) {

        this.getIntegrations()
            .subscribe(result => {
                result.data.forEach(plugin => {
                    if (!plugin.versionDetails.version || plugin.versionDetails.version &&
                            plugin.versionDetails.version !== '&nbsp;' &&
                            plugin.versionDetails.version.indexOf('v.') !== 0) {
                        plugin.versionDetails.version = (plugin.versionDetails.version) ? 'v.&nbsp;' + plugin.versionDetails.version : '&nbsp;';
                    }

                    plugin.information.platforms.icons = this.setPlatformIcons(plugin);
                    plugin.information.logo = plugin.information.logo || this.config.icons.default;

                    plugin.state = (plugin.pending) ? 'pending' : (plugin.draft) ? 'draft' : undefined;

                    plugin.link = '/integrations/' + plugin.id;
                    plugin.link += (plugin.state) ? '?state=' + plugin.state : '';
                });
                this.pluginsSubject.next(result.data);
            });

        this.config = this.configService.getConfig();
    }

    private getIntegrations(): Observable<any> {
        return this.api.getIntegrations();
    }

    private setScreenshots(section) {
        if (section) {
            section.screenshots = Object.keys(section).filter((element) => {
                return element.match(/screenshot/i) && section[element];
            }).sort().map((key) => {
                return {id: key, value: section[key]};
            });
            if (section.screenshots.length < 1) {
                delete section.screenshots;
            }
        }
    }

    private formatScreenshots(plugin) {
        const processed: any = [];

        Object.entries(plugin.overview).forEach((item) => {
            const matchScreenshot = item[0].match(/Screenshot[\d]+$/);

            if (matchScreenshot) {
                processed.push({ id: item[0].replace('overview', ''), value: item[1] });
            }
        });

        Object.entries(plugin.overview).forEach((item) => {
            const matchCaption = item[0].match(/Screenshot[\d]+caption$/);

            if (matchCaption) {
                processed.find((i) => {
                    if (i.id === matchCaption[0].replace('caption', '')) {
                        i.caption = item[1];
                    }
                });
            }
        });

        plugin.overview.screenshots = processed;
    }

    setPlatformIcons(plugin) {
        const platformIcons = [];

        this.config.icons.platforms.forEach(icon => {
            const platform = plugin.information.platforms.find(platform => {
                // 32 or 64 bit? ... it doesn't matter :)
                return platform.toLowerCase().indexOf(icon.name) > -1;
            });
            if (platform) {
                platformIcons.push({ name: platform, src: icon.src });
            }
        });

        return platformIcons;
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
                    platform.name = this.config.defaultPlatformNames[platformName];
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

        plugin.versionDetails.version = (plugin.versionDetails.version) ? 'v.&nbsp;' + plugin.versionDetails.version : '&nbsp;';
        plugin.information.platforms.icons = this.setPlatformIcons(plugin);

        this.setScreenshots(plugin.instructions);
        this.formatScreenshots(plugin);

        return plugin;
    }

    getIntegrationBy(id: number, status: string): Observable<any> {
        return this.api.getIntegrationBy(id, status);
    }

    setIntegrationPlugin(plugin: any = {}) {
        this.plugin = plugin;
    }

    getIntegrationPlugin() {
        return this.plugin;
    }

    setSection(section) {
        this.selectedSectionSubject.next(section);
    }

    ngOnDestroy() {
        this.pluginsSubject.unsubscribe();
    }
}
