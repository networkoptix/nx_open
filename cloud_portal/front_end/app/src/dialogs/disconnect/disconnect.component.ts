import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { Location }                                            from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }               from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                      from '@angular/forms';

@Component({
    selector: 'nx-modal-disconnect-content',
    templateUrl: 'disconnect.component.html',
    styleUrls: []
})
export class DisconnectModalContent {
    @Input() systemId;
    @Input() language;
    @Input() disconnect;
    @Input() closable;

    password: string;

    constructor(private activeModal: NgbActiveModal,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any,) {
    }

    ngOnInit() {
        this.disconnect = this.process.init(() => {
            return this.cloudApi.disconnect(this.systemId, this.password);
        }, {
            ignoreUnauthorized: true,
            errorCodes: {
                notAuthorized: this.language.errorCodes.passwordMismatch
            },
            successMessage: this.language.system.successDisconnected,
            errorPrefix: this.language.errorCodes.cantDisconnectSystemPrefix
        }).then(() => {
            this.activeModal.close('OK');
        });
    }

    close() {
        console.log('TEST');
        this.activeModal.close('CLOSE');
    }
}

@Component({
    selector: 'nx-modal-disconnect',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalDisconnectComponent implements OnInit {
    modalRef: NgbModalRef;
    disconnect: any;

    constructor(@Inject('languageService') private language: any,
                private location: Location,
                private modalService: NgbModal) {
    }

    private dialog(systemId) {
        this.modalRef = this.modalService.open(DisconnectModalContent, {backdrop: 'static', centered: true});
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.disconnect = this.disconnect;
        this.modalRef.componentInstance.systemId = systemId;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(systemId) {
        return this.dialog(systemId).result;
    }

    ngOnInit() {
    }
}
