import { Component, OnInit, OnDestroy, SimpleChanges } from '@angular/core';
import { ActivatedRoute } from '@angular/router';
import { IntegrationService } from '../integration.service';
import { DomSanitizer } from '@angular/platform-browser';

@Component({
    selector: 'integration-detail-component',
    templateUrl: 'details.component.html',
    styleUrls: ['details.component.scss']
})

export class NxIntegrationDetailsComponent implements OnInit, OnDestroy {

    plugin: any;

    private setupDefaults() {
    }

    constructor(public sanitizer: DomSanitizer,
                private _route: ActivatedRoute,
                private integrationService: IntegrationService) {

        this.setupDefaults();
    }

    ngOnInit(): void {
        this.integrationService
                .pluginsSubject
                .subscribe(res => {
                    this._route.params.subscribe(params => {
                        this.integrationService.getPluginBy(params.plugin);
                        this.integrationService.selectedPluginSubject.subscribe(plugin => {
                            this.plugin = plugin;
                        });
                    });
                });
    }

    ngOnDestroy() {
    }

    onSubmit() {
    }
}

