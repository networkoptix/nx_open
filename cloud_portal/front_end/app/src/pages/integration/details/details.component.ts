import { Component, OnDestroy, OnInit } from '@angular/core';
import { ActivatedRoute }               from '@angular/router';
import { IntegrationService }           from '../integration.service';
import { DomSanitizer }                 from '@angular/platform-browser';
import { NxRibbonService }              from '../../../components/ribbon/ribbon.service';
import { NxConfigService }              from '../../../services/nx-config';
import { NxModalMessageComponent }      from '../../../dialogs/message/message.component';
import { NxLanguageProviderService }    from '../../../services/nx-language-provider';
import { TranslateService }             from '@ngx-translate/core';

@Component({
    selector   : 'integration-detail-component',
    templateUrl: 'details.component.html',
    styleUrls  : ['details.component.scss']
})

export class NxIntegrationDetailsComponent implements OnInit, OnDestroy {

    plugin: any;
    config: any = {};
    lang: any = {};

    private setupDefaults() {
        this.config = this.configService.getConfig();
        this.language
            .translationsSubject
            .subscribe((lang) => {
                this.lang = lang;
            });
    }

    constructor(public sanitizer: DomSanitizer,
                private _route: ActivatedRoute,
                private integrationService: IntegrationService,
                private ribbonService: NxRibbonService,
                private configService: NxConfigService,
                // TODO: Use dialog service when it is not being downgraded
                private messageDialog: NxModalMessageComponent,
                private language: NxLanguageProviderService,
                private translate: TranslateService) {

        this.setupDefaults();
    }

    ngOnInit(): void {
        this._route.params.subscribe(params => {
            this.integrationService.getIntegrationBy(params.pluginId, params.status)
                .subscribe(result => {
                    if (result.length) {
                        this.plugin = this.integrationService.format(result[0]);
                        if (this.plugin && (this.plugin.pending && this.plugin.draft)) {
                            this.ribbonService.show(
                                    this.lang[this.translate.currentLang].integration.previewRibbonText,
                                    this.lang[this.translate.currentLang].integration.backToEditText,
                                    this.config.links.admin.product.replace('%ID%', this.plugin.id)
                            );
                        }

                        if (this.plugin.overview) {
                            this.plugin.overview.screenshots = this.integrationService.formatScreenshots(this.plugin.overview);
                        }
                    }
                });
        });
    }

    ngOnDestroy() {
        this.ribbonService.hide();
        this.plugin = undefined;
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

