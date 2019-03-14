import { Component, Inject, OnInit, Input, ViewEncapsulation, Renderer2 } from '@angular/core';
import { Location }                                                       from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }                          from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                                 from '@angular/forms';

@Component({
    selector: 'nx-modal-rename-content',
    templateUrl: 'rename.component.html',
    styleUrls: []
})
export class RenameModalContent {
    @Input() systemId;
    @Input() systemName;
    @Input() language;
    @Input() closable;

    rename: any;

    constructor(private activeModal: NgbActiveModal,
                private renderer: Renderer2,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any) {

    }

    ngOnInit() {
        this.rename = this.process.init(() => {
            return this.cloudApi.renameSystem(this.systemId, this.systemName);
        }, {
            successMessage: this.language.system.successRename
        }).then(() => {
            this.activeModal.close(this.systemName);
        });
    }

    close() {
        this.activeModal.close();
    }
}

@Component({
    selector: 'nx-modal-rename',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalRenameComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(@Inject('languageService') private language: any,
                private modalService: NgbModal) {
    }

    private dialog(systemId, systemName) {
        // TODO: Refactor dialog to use generic dialog
        // TODO: retire loading ModalContent (CLOUD-2493)
        this.modalRef = this.modalService.open(RenameModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.systemId = systemId;
        this.modalRef.componentInstance.systemName = systemName;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(systemId, systemName) {
        return this.dialog(systemId, systemName).result;
    }

    ngOnInit() {
    }
}
