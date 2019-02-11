import {
    Component, OnDestroy,
    Input, SimpleChanges, OnChanges
} from '@angular/core';

import { NxConfigService }           from '../../../services/nx-config';
import { NxRibbonService }           from '../../../components/ribbon/ribbon.service';
import { TranslateService }          from '@ngx-translate/core';
import { NxLanguageProviderService } from '../../../services/nx-language-provider';

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
                private ribbonService: NxRibbonService,
                private language: NxLanguageProviderService) {

        this.setupDefaults();
    }

    getPlatformIconsFor(plugin) {

        this.defaultLogo = this.config.icons.default;

        const platformIcons = [];

        this.config.icons.platforms.forEach(icon => {
            const platform = plugin.information.platforms.find(platform => {
                // 32 or 64 bit? ... it doesn't matter :)
                return platform.toLowerCase().indexOf(icon.name) > -1;
            });

            if (platform) {
                platformIcons.push({ name: platform, src: icon.src });
            }
        });

        return platformIcons;
    }

    setPlugunLogo(plugin) {
        plugin.information.logo = plugin.information.logo || this.defaultLogo;
    }

    ngOnDestroy() {
        this.ribbonService.hide();
    }

    ngOnChanges(changes: SimpleChanges) {
        let haveInReview = false;
        if (changes.list) {
            // inject platform icons info
            this.list.forEach(plugin => {
                plugin.information.platforms.icons = this.getPlatformIconsFor(plugin);
                this.setPlugunLogo(plugin);

                haveInReview = haveInReview || plugin.pending;
            });

            if (haveInReview) {
                this.ribbonService.show(
                        this.lang.integration.previewRibbonText,
                        this.lang.integration.backToEditText,
                        this.config.links.admin.product.replace('%ID%/pages/', '')
                );
            }
        }
    }
}

