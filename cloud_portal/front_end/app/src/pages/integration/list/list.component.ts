import {
    Component, OnDestroy,
    Input, SimpleChanges, OnChanges
} from '@angular/core';

import { NxConfigService }           from '../../../services/nx-config';
import { NxRibbonService }           from '../../../components/ribbon/ribbon.service';
import { TranslateService }          from '@ngx-translate/core';
import { NxLanguageProviderService } from '../../../services/nx-language-provider';
import { IntegrationService }        from '../integration.service';

@Component({
    selector   : 'integrations-list-component',
    templateUrl: 'list.component.html',
    styleUrls  : ['list.component.scss']
})

export class NxIntegrationsListComponent implements OnDestroy, OnChanges {

    @Input() list;

    config: any;
    lang: any;

    private setupDefaults() {
        this.config = this.configService.getConfig();
        this.language
            .translationsSubject
            .subscribe((lang) => {
                this.lang = lang;
            });
    }

    constructor(private configService: NxConfigService,
                private integrations: IntegrationService,
                private ribbonService: NxRibbonService,
                private language: NxLanguageProviderService,
                private translate: TranslateService) {

        this.setupDefaults();
    }

    ngOnDestroy() {
        this.ribbonService.hide();
    }

    ngOnChanges(changes: SimpleChanges) {
        let haveInReviewOrDraft = false;
        if (changes.list.currentValue) {
            // inject platform icons info
            changes.list.currentValue.some(plugin => {
                if (plugin.pending || plugin.draft) {
                    haveInReviewOrDraft = true;
                    return true;
                }
            });

            if (haveInReviewOrDraft) {
                this.ribbonService.show(
                        this.lang[this.translate.currentLang].integration.previewRibbonText,
                        this.lang[this.translate.currentLang].integration.backToEditText,
                        this.config.links.admin.product.replace('%ID%/pages/', '')
                );
            }
        }
    }
}

