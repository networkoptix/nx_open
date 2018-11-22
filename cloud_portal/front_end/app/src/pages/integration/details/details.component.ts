import { Component, OnInit, OnDestroy, SimpleChanges } from '@angular/core';
import { ActivatedRoute } from '@angular/router';
import { IntegrationService } from '../integration.service';
import { DomSanitizer } from '@angular/platform-browser';
import { NxRibbonService } from '../../../components/ribbon/ribbon.service';
import { NxConfigService } from '../../../services/nx-config';
import { TranslateService } from '@ngx-translate/core';

@Component({
    selector: 'integration-detail-component',
    templateUrl: 'details.component.html',
    styleUrls: ['details.component.scss']
})

export class NxIntegrationDetailsComponent implements OnInit, OnDestroy {

    plugin: any;
    config: any;

    private setupDefaults() {
        this.config = this.configService.getConfig();
    }

    constructor(public sanitizer: DomSanitizer,
                private _route: ActivatedRoute,
                private integrationService: IntegrationService,
                private ribbonService: NxRibbonService,
                private configService: NxConfigService,
                private translate: TranslateService) {

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

                            if (this.plugin.pending) {
                                this.translate
                                        .get([
                                            'This page is a preview of the latest changes, and it doesn\'t match publicly available version.',
                                            'Back to the editing interfaces'
                                        ])
                                        .subscribe((res: string) => {
                                            this.ribbonService.show(
                                                    res['This page is a preview of the latest changes, and it doesn\'t match publicly available version.'],
                                                    res['Back to the editing interfaces'],
                                                    this.config.links.admin.product
                                            );
                                        });
                            }
                        });
                    });
                });
    }

    ngOnDestroy() {
        this.ribbonService.hide();
    }

    onSubmit() {
    }
}

