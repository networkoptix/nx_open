import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { Location }                                            from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }               from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                      from '@angular/forms';

@Component({
    selector: 'nx-modal-rename-content',
    templateUrl: 'rename.component.html',
    styleUrls: []
})
export class RenameModalContent {
    @Input() systemId;
    @Input() systemName;
    @Input() language;

    renaming: any;

    constructor(public activeModal: NgbActiveModal,
                @Inject('process') private process: any,
                @Inject('cloudApiService') private cloudApi: any) {

    }

    ngOnInit() {
        this.renaming = this.process.init(function () {
            return this.cloudApi.renameSystem(this.systemId, this.systemName);
        }, {
            successMessage: this.language.system.successRename
        }).then(function () {
            this.activeModal.close();
        });
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
        this.modalRef = this.modalService.open(RenameModalContent);
        this.modalRef.componentInstance.language = this.language;
        this.modalRef.componentInstance.systemId = systemId;
        this.modalRef.componentInstance.systemName = systemName;

        return this.modalRef;
    }

    open(systemId, systemName) {
        return this.dialog(systemId, systemName);
    }

    close() {
        this.modalRef.close();
    }

    ngOnInit() {
    }
}
