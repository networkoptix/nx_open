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
                        this.plugin = this.integrationService.getPluginBy(params.plugin);

                        if (this.plugin) {
                            // build static info if properties of plugin are populated
                            // to simplify blocks vivsibility
                            for (const param in this.plugin) {
                                if (this.plugin.hasOwnProperty(param)) {
                                    if (Object.keys(this.plugin[param]).length) {
                                        this.plugin[param].hasInfo = false;
                                    }
                                    for (const key in this.plugin[param]) {
                                        if (this.plugin[param].hasOwnProperty(key)) {
                                            if (key !== 'hasInfo' && this.plugin[param][key] !== '') {
                                                this.plugin[param].hasInfo = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    });
                });
    }

    ngOnDestroy() {
    }

    onSubmit() {
    }
}

