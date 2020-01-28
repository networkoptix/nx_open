import {
    Component,
    OnInit,
    Input,
    ViewEncapsulation, Inject, OnDestroy
} from '@angular/core';
import { Router }                    from '@angular/router';
import { NxConfigService }           from '../../services/nx-config';
import { NxDialogsService }          from '../../dialogs/dialogs.service';
import { NxUrlProtocolService }      from '../../services/url-protocol.service';
import { NxLanguageProviderService } from '../../services/nx-language-provider';
import { NxProcessService }          from '../../services/process.service';

@Component({
    selector     : 'nx-client-button',
    templateUrl  : 'client-button.component.html',
    styleUrls    : ['client-button.component.scss'],
    encapsulation: ViewEncapsulation.None
})
export class NxClientButtonComponent implements OnInit, OnDestroy {

    @Input() system: any;
    @Input() customClass: any;
    @Input() actionType: any;

    CONFIG: any = {};
    LANG: any = {};

    location: any;
    canceled: boolean;
    modalActive: boolean;
    openClient: any;

    constructor(private processService: NxProcessService,
                private urlProtocol: NxUrlProtocolService,
                private config: NxConfigService,
                private language: NxLanguageProviderService,
                private dialogs: NxDialogsService,
                private router: Router) {

        this.location = location;
    }

    ngOnDestroy(): void {
        this.canceled = true;
    }

    ngOnInit() {
        this.CONFIG = this.config.getConfig();
        this.LANG = this.language.getTranslations();
        this.modalActive = false;
        this.canceled = false;

        this.openClient = this.processService.createProcess(() => {
            return this.urlProtocol
                .open(this.system && this.system.id);
        }, {
            errorCodes: {
                notVisited: () => false
            }
        }).then(() => {
            this.modalActive = false;
        }, () => {
            // message, title, actionLabel, actionType
            if (this.modalActive || this.canceled) {
                return;
            }
            this.modalActive = true;
            return this.dialogs
                .confirm(
                    this.LANG.errorCodes.cantOpenClient,
                    this.LANG.dialogs.noClientDetectedTitle,
                    this.LANG.dialogs.download,
                    'btn-primary',
                    this.LANG.dialogs.cancelButton
                )
                .then((result) => {
                    if (result === true) {
                        this.router.navigate(['/download']);
                    }
                }).finally(() => {
                    this.modalActive = false;
                });
            });
    }
}
