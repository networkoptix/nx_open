import { Component, OnInit, OnDestroy, Input } from '@angular/core';
import { ActivatedRoute }                      from '@angular/router';
import { IntegrationService }                  from '../integration.service';

@Component({
    selector: 'integration-detail-component',
    templateUrl: 'details.component.html',
    styleUrls: ['details.component.scss']
})

export class NxIntegrationDetailsComponent implements OnInit, OnDestroy {

    plugin: any;
    defaultLogo: string;

    private setupDefaults() {
        this.defaultLogo = '/static/icons/integration_tile_preview_plugin.svg';
    }

    constructor(private _route: ActivatedRoute,
                private integrationService: IntegrationService) {

        this.setupDefaults();
    }

    ngOnInit(): void {
        this.integrationService
                .pluginsSubject
                .subscribe(res => {
                    this._route.params.subscribe(params => {
                        this.plugin = this.integrationService.getPluginBy(params.plugin);
                    });
                });
    }

    ngOnDestroy() {
    }

    onSubmit() {
    }
}

