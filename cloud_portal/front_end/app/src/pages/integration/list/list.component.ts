import { Component, OnInit, OnDestroy, Input } from '@angular/core';
import { NxConfigService } from '../../../services/nx-config';

@Component({
    selector: 'integrations-list-component',
    templateUrl: 'list.component.html',
    styleUrls: ['list.component.scss']
})

export class NxIntegrationsListComponent implements OnInit, OnDestroy {

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

    isSupported(plugin, os) {
        const result = plugin.information.platforms.find(platform => {
            // 32 or 64 bit? ... it doesn't matter :)
            return platform.toLowerCase().indexOf(os) > -1;
        });

        return result !== undefined;
    }

    ngOnInit(): void {

    }

    ngOnDestroy() {
    }

    onSubmit() {
    }
}

