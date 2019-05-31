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

    defaultLogo: string;
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

    setPlugunLogo(plugin) {
        plugin.information.logo = plugin.information.logo || this.defaultLogo;
    }

    ngOnDestroy() {
        this.ribbonService.hide();
    }

    ngOnChanges(changes: SimpleChanges) {
        let haveInReviewOrDraft = false;
        if (changes.list.currentValue) {
            // inject platform icons info
            changes.list.currentValue.forEach(plugin => {
                plugin.information.platforms.icons = this.integrations.setPlatformIcons(plugin);
                this.setPlugunLogo(plugin);

                haveInReviewOrDraft = haveInReviewOrDraft || plugin.pending || plugin.draft;
                plugin.state = (plugin.pending) ? 'pending' : (plugin.draft) ? 'draft' : undefined;

                plugin.link = '/integrations/' + plugin.id;
                plugin.link += (plugin.state) ? '?state=' + plugin.state : '';
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

