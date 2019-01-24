import {
    Component, OnInit, OnDestroy,
    Input, SimpleChanges, OnChanges
} from '@angular/core';

import { NxConfigService } from '../../../services/nx-config';
import { NxRibbonService } from '../../../components/ribbon/ribbon.service';
import { TranslateService } from '@ngx-translate/core';

@Component({
    selector: 'integrations-list-component',
    templateUrl: 'list.component.html',
    styleUrls: ['list.component.scss']
})

export class NxIntegrationsListComponent implements OnInit, OnDestroy, OnChanges {

    @Input() list;

    defaultLogo: string;
    config: any;
    lang: any;

    private setupDefaults() {
        this.config = this.configService.getConfig();
        this.lang = this.translate.translations[this.translate.currentLang];
    }

    constructor(private configService: NxConfigService,
                private ribbonService: NxRibbonService,
                private translate: TranslateService) {

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

    ngOnInit(): void {
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
                        this.lang.integration_preview,
                        this.lang.integration_back_to_edit,
                        this.config.links.admin.product
                );
            }
        }
    }
}

