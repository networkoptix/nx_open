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
    content: any = {};

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
        this.integrationService
            .selectedSectionSubject
            .subscribe(selection => {
                this.content.selectedSection = selection;
                this.content = {...this.content}; // trigger onChange
        });

        this._route.params.subscribe(params => {
            if (params.id) {

                this.integrationService.setIntegrationPlugin({});
                this.content = {
                    selectedSection: '',        // updated by selectedSectionSubject
                    base  : '/integrations/',   // updated by route param
                    level1: [
                        {
                            id    : '',
                            label : '',
                            path  : '',
                            level2: [
                                {
                                    id   : 'how-it-works',
                                    label: 'How it works',
                                    path : 'how-it-works',
                                },
                                {
                                    id   : 'how-to-install',
                                    label: 'How to install?',
                                    path : 'how-to-install',
                                }]
                        }]
                };

                this.integrationService.getIntegrationBy(params.id, params.status)
                    .subscribe(result => {
                        if (result.length) {
                            this.content.base += params.id + '/' + params.state;

                            this.plugin = this.integrationService.format(result[0]);

                            if (this.plugin && (this.plugin.pending && this.plugin.draft)) {
                                this.ribbonService.show(
                                        this.lang[this.translate.currentLang].integration.previewRibbonText,
                                        this.lang[this.translate.currentLang].integration.backToEditText,
                                        this.config.links.admin.product.replace('%ID%', this.plugin.id)
                                );
                            }

                            this.integrationService.setIntegrationPlugin(this.plugin);
                        }
                    });
            }
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

