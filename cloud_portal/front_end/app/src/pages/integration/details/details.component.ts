import { Component, OnDestroy, AfterViewInit } from '@angular/core';
import { ActivatedRoute }                      from '@angular/router';
import { IntegrationService }                  from '../integration.service';
import { DomSanitizer }                        from '@angular/platform-browser';
import { NxRibbonService }                     from '../../../components/ribbon/ribbon.service';
import { NxConfigService }                     from '../../../services/nx-config';
import { TranslateService }                    from '@ngx-translate/core';
import { NxModalMessageComponent }             from '../../../dialogs/message/message.component';

@Component({
    selector   : 'integration-detail-component',
    templateUrl: 'details.component.html',
    styleUrls  : ['details.component.scss']
})

export class NxIntegrationDetailsComponent implements AfterViewInit, OnDestroy {

    plugin: any;
    config: any;
    lang: any;

    private setupDefaults() {
        this.lang = this.translate.translations[this.translate.currentLang];
        this.config = this.configService.getConfig();
    }

    constructor(public sanitizer: DomSanitizer,
                private _route: ActivatedRoute,
                private integrationService: IntegrationService,
                private ribbonService: NxRibbonService,
                private configService: NxConfigService,
                // TODO: Use dialog service when it is not being downgraded
                private messageDialog: NxModalMessageComponent,
                private translate: TranslateService) {

        this.setupDefaults();
    }

    ngAfterViewInit(): void {
        setTimeout(() => {
            // on reload language service is initialized later
            if (!this.lang) {
                this.lang = this.translate.translations[this.translate.currentLang];
            }
            this.integrationService
                .pluginsSubject
                .subscribe(res => {
                    this._route.params.subscribe(params => {
                        this.integrationService.getPluginBy(params.plugin);
                        this.integrationService.selectedPluginSubject.subscribe(plugin => {
                            this.plugin = plugin;

                            if (this.plugin && this.plugin.pending) {
                                this.ribbonService.show(
                                        this.lang.integration_preview,
                                        this.lang.integration_back_to_edit,
                                        this.config.links.admin.product.replace('%ID%', this.plugin.id)
                                );
                            }
                        });
                    });
                });
        });
    }

    ngOnDestroy() {
        this.ribbonService.hide();
    }

    requestSupport() {
        this.messageDialog.open(this.config.messageType.support, this.plugin.information.name, this.plugin.id).then(() => {
        });
    }

    sendInquiry() {
        this.messageDialog.open(this.config.messageType.inquiry, this.plugin.information.name, this.plugin.id).then(() => {
        });
    }
}

