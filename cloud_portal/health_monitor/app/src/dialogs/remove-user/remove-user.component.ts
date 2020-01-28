import {
    Component, Input, OnInit, Renderer2, ViewEncapsulation
}                                                from '@angular/core';
import { NgbActiveModal, NgbModal, NgbModalRef } from '@ng-bootstrap/ng-bootstrap';
import { NxConfigService }                       from '../../services/nx-config';
import { NxLanguageProviderService }             from '../../services/nx-language-provider';
import { NxProcessService }                      from '../../services/process.service';

@Component({
    selector   : 'nx-modal-remove-user-content',
    templateUrl: 'remove-user.component.html',
    styleUrls  : []
})
export class RemoveUserModalContent {
    @Input() system;
    @Input() user;
    @Input() closable;

    LANG: any;
    CONFIG: any;

    removeUserProcess: any;

    constructor(public activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private configService: NxConfigService,
                private language: NxLanguageProviderService,
                private processService: NxProcessService
    ) {
        this.CONFIG = this.configService.getConfig();
        this.LANG = this.language.getTranslations();
    }

    ngOnInit() {
        this.removeUserProcess = this.processService.createProcess(() => {
            return this.system.deleteUser(this.user).then(() => {
                return this.system.getUsers(true);
            });
        }, {
            successMessage: this.LANG.system.permissionsRemoved.replace('{{email}}', this.user ? this.user.email : ''),
            errorPrefix   : this.LANG.errorCodes.cantSharePrefix
        }).then(() => {
            this.activeModal.close(true);
        });
    }

    close() {
        this.activeModal.close();
    }
}

@Component({
    selector     : 'nx-modal-remove-user',
    template     : '',
    encapsulation: ViewEncapsulation.None,
    styleUrls    : []
})
export class NxModalRemoveUserComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(private modalService: NgbModal) {
    }

    private dialog(system?, user?) {
        // TODO: Refactor dialog to use generic dialog
        // TODO: retire loading ModalContent (CLOUD-2493)
        this.modalRef = this.modalService.open(RemoveUserModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });

        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.user = user;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(system?, user?) {
        return this.dialog(system, user).result;
    }

    ngOnInit() {
    }
}
