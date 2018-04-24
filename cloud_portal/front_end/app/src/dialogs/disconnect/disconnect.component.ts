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

    password: string;
    disconnecting: any;

    constructor(public activeModal: NgbActiveModal,
                @Inject('account') private account: any,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any,) {

    }

    ngOnInit() {
        this.disconnecting = this.process.init(() => {
            return this.cloudApi.disconnect(this.systemId, this.password);
        }, {
            ignoreUnauthorized: true,
            errorCodes: {
                notAuthorized: this.language.errorCodes.passwordMismatch
            },
            successMessage: this.language.system.successDisconnected,
            errorPrefix: this.language.errorCodes.cantDisconnectSystemPrefix
        }).then(function () {
            this.activeModal.close();
        });
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

    constructor(@Inject('languageService') private language: any,
                private location: Location,
                private modalService: NgbModal) {
    }

    private dialog(systemId) {
        this.modalRef = this.modalService.open(DisconnectModalContent);
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.systemId = systemId;

        return this.modalRef;
    }

    open(systemId) {
        return this.dialog(systemId);
    }

    close() {
        this.modalRef.close();
    }

    ngOnInit() {
    }
}
