import { Component, Input, OnInit, ViewEncapsulation } from '@angular/core';
import { NxLanguageProviderService }                   from '../../../services/nx-language-provider';
import { NxConfigService } from '../../../services/nx-config';

/* Usage
<nx-page-placeholder
     type?="500 | 404 | NO_ALERTS | OFFLINE | ..."
     -- OR ---
     iconClass?='server-offline'
     placeholderTitle?='SERVER OFFLINE'
     message?='Warning! Dragons ahead!'
     preloader?=BOOLEAN
     [condition]= WHEN_TO_SHOW >
</nx-page-placeholder>
*/

@Component({
    selector: 'nx-page-placeholder',
    templateUrl: 'page-placeholder.component.html',
    styleUrls: ['page-placeholder.component.scss'],
    encapsulation: ViewEncapsulation.None
})
export class NxPagePlaceholderComponent implements OnInit {
    @Input() type: string;
    @Input() iconClass: string;
    @Input() placeholderTitle: string;
    @Input() message: string;
    @Input() preloader: boolean;
    @Input() condition: boolean;

    CONFIG: any;
    LANG: any;

    iconName: string;

    constructor(private configService: NxConfigService,
                private languageService: NxLanguageProviderService,
    ) {
        this.CONFIG = this.configService.getConfig();
        this.LANG = this.languageService.getTranslations();
    }

    ngOnInit() {
        if (this.type) {
            if (!this.preloader && !this.condition) {
                this.preloader = false;
                this.condition = true;
            }

            switch (this.type) {
                case 'OFFLINE' :
                    this.placeholderTitle = this.LANG.common.systemOffline;
                    this.message = this.LANG.common.systemOfflineMessage;
                    this.iconName = 'Offline';
                    break;
                case 'NO_ALERTS' :
                    this.placeholderTitle = this.LANG.common.systemNoAlerts;
                    this.message = this.LANG.common.systemNoAlertsMessage;
                    this.iconName = 'NoActions';
                    break;
                case '500' :
                    this.placeholderTitle = this.LANG.common.systemServerError;
                    this.message = this.LANG.common.systemServerErrorMessage;
                    this.iconName = '500';
                    break;
                case 'NEW_VERSION' :
                    this.placeholderTitle = this.LANG.common.systemNewVersion;
                    this.message = this.LANG.common.systemNewVersionMessage;
                    this.iconName = 'NewVersion';
                    break;
            }
        }
    }
}
