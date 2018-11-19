import {
    Component, OnInit, OnDestroy,
    Input, SimpleChanges, OnChanges
} from '@angular/core';

import { NxConfigService } from '../../../services/nx-config';

@Component({
    selector: 'integrations-list-component',
    templateUrl: 'list.component.html',
    styleUrls: ['list.component.scss']
})

export class NxIntegrationsListComponent implements OnInit, OnDestroy, OnChanges {

    @Input() list;

    defaultLogo: string;
    config: any;

    private setupDefaults() {
        this.defaultLogo = '/static/icons/integration_tile_preview_plugin.svg';
    }

    constructor(configService: NxConfigService) {
        this.setupDefaults();
        this.config = configService.getConfig();
    }

    getPlatformIconsFor(plugin) {
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

    setPlugunLogo(plugin) {
        plugin.information.logo = plugin.information.logo || this.defaultLogo;
    }

    ngOnInit(): void {
    }

    ngOnDestroy() {
    }

    ngOnChanges(changes: SimpleChanges) {
        if (changes.list) {
            // inject platform icons info
            this.list.forEach((plugin) => {
                plugin.information.platforms.icons = this.getPlatformIconsFor(plugin);
                this.setPlugunLogo(plugin);
            });
        }
    }
}

