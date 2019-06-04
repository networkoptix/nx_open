import { Component, OnDestroy, OnInit } from '@angular/core';
import { Location }                     from '@angular/common';
import { ActivatedRoute }               from '@angular/router';
import { IntegrationService }        from '../integration.service';
import { DomSanitizer }              from '@angular/platform-browser';
import { NxRibbonService }           from '../../../components/ribbon/ribbon.service';
import { NxConfigService }           from '../../../services/nx-config';
import { NxModalMessageComponent }   from '../../../dialogs/message/message.component';
import { NxLanguageProviderService } from '../../../services/nx-language-provider';
import { TranslateService }          from '@ngx-translate/core';

import { map }           from 'rxjs/operators';
import { combineLatest } from 'rxjs';

@Component({
    selector   : 'integration-detail-component',
    templateUrl: 'details.component.html',
    styleUrls  : ['details.component.scss']
})

export class NxIntegrationDetailsComponent implements OnInit, OnDestroy {

    plugin: any;
    config: any = {};
    content: any = {};
    lang: any = {};
    location: any;

    private setupDefaults() {
        this.config = this.configService.getConfig();
        this.language
            .translationsSubject
            .subscribe((lang) => {
                this.lang = lang;
            });


    }

    constructor(public sanitizer: DomSanitizer,
                private route: ActivatedRoute,
                private integrationService: IntegrationService,
                private ribbonService: NxRibbonService,
                private configService: NxConfigService,
                // TODO: Use dialog service when it is not being downgraded
                private messageDialog: NxModalMessageComponent,
                private language: NxLanguageProviderService,
                private translate: TranslateService,
                location: Location) {
        this.location = location;
        this.setupDefaults();
    }

    ngOnInit(): void {
        this.integrationService
            .selectedSectionSubject
            .subscribe(selection => {
                this.content.selectedSection = selection;
                this.content = {...this.content}; // trigger onChange
        });

        combineLatest(this.route.params, this.route.queryParams)
                .pipe(map(results => ({ params: results[0], query: results[1] })))
                .subscribe(results => {

                    // @ts-ignore
                    if (results.params.id) {

                        this.integrationService.setIntegrationPlugin({});

                        // @ts-ignore
                        const query = results.query;

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
                                            // path : 'how-it-works',
                                            path : '',
                                            query
                                        },
                                        {
                                            id   : 'how-to-install',
                                            label: 'How to install?',
                                            path : 'how-to-install',
                                            query
                                        }]
                                }]
                        };

                        // @ts-ignore
                        this.integrationService.getIntegrationBy(results.params.id, results.query.state)
                            .subscribe(result => {
                                if (result.length) {
                                    // @ts-ignore
                                    this.content.base += results.params.id;

                                    this.plugin = this.integrationService.format(result[0]);

                                    if (this.plugin.pending || this.plugin.draft) {
                                        this.ribbonService.show(
                                                this.lang[this.translate.currentLang].integration.previewRibbonText,
                                                this.lang[this.translate.currentLang].integration.backToEditText,
                                                this.config.links.admin.product.replace('%ID%', this.plugin.id)
                                        );
                                    }

                                    this.integrationService.setIntegrationPlugin(this.plugin);
                                }
                            }).add(() => {
                                if (!this.plugin) {
                                    this.location.go('404');
                                }
                            });
                    }
                });
    }

    ngOnDestroy() {
        this.ribbonService.hide();
        this.plugin = undefined;
    }

    openMessageDialog() {
        this.messageDialog.open(this.config.messageType.integration, this.plugin.information.name, this.plugin.id).then(() => {});
    }
}

