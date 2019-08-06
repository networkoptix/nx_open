import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { Location } from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef } from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator } from '@angular/forms';

@Component({
    selector: 'nx-modal-generic-content',
    templateUrl: 'generic.component.html',
    styleUrls: []
})
export class GenericModalContent implements OnInit {
    @Input() language;
    @Input() message;
    @Input() title;
    @Input() actionLabel;
    @Input() buttonType;
    @Input() cancelLabel;
    @Input() buttonClass;
    @Input() hasFooter;
    @Input() cancellable;
    @Input() closable;

    constructor(public activeModal: NgbActiveModal) {
    }

    ngOnInit() {
        this.buttonClass = this.buttonClass || '';
        this.closable = !!this.closable;
    }

    ok() {
        this.activeModal.close(true);
    }

    close() {
        this.activeModal.close();
    }
}

@Component({
    selector: 'nx-modal-general',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalGenericComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(@Inject('languageService') private language: any,
                private location: Location,
                private modalService: NgbModal) {
    }

    private dialog(message, title, actionLabel, actionType?, cancelLabel?,
                   hasFooter?, cancellable?, closable?) {
        this.modalRef = this.modalService.open(GenericModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });
        this.modalRef.componentInstance.language = this.language.lang;

        this.modalRef.componentInstance.message = message;
        this.modalRef.componentInstance.title = title;
        this.modalRef.componentInstance.actionLabel = actionLabel;
        this.modalRef.componentInstance.buttonType = actionType || 'default';
        this.modalRef.componentInstance.cancelLabel = cancelLabel;
        this.modalRef.componentInstance.buttonClass = actionType || 'btn-primary';

        this.modalRef.componentInstance.hasFooter = hasFooter;
        this.modalRef.componentInstance.cancellable = cancellable;
        this.modalRef.componentInstance.closable = closable;

        return this.modalRef;
    }

    openConfirm(message, title, actionLabel, actionType?, cancelLabel?) {
        return this.dialog(message, title, actionLabel,
            actionType,
            cancelLabel,
            true,
            false,
            true)
            .result;
    }

    openAlert(message, title) {
        return this.dialog(message, title,
            this.language.lang.dialogs.okButton,
            null,
            this.language.lang.dialogs.cancelButton,
            true,
            true,
            true)
            .result;

    }

    ngOnInit() {
    }
}
