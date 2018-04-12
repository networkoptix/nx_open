import {Component, Inject, OnInit, Input, ViewEncapsulation} from '@angular/core';
import {Location} from '@angular/common';
import {NgbModal, NgbActiveModal, NgbModalRef} from '@ng-bootstrap/ng-bootstrap';
import {EmailValidator} from '@angular/forms';

@Component({
    selector: 'nx-modal-general-content',
    templateUrl: 'general.component.html',
    styleUrls: []
    // TODO: later
    // templateUrl: this.CONFIG.viewsDir + 'components/dialog.html'
})
export class GeneralModalContent {
    @Input() message;
    @Input() title;
    @Input() actionLabel;
    @Input() actionType;
    @Input() cancelLabel;
    @Input() hasFooter;
    @Input() cancellable;
    @Input() closable;

    constructor(public activeModal: NgbActiveModal) {
    }
}

@Component({
    selector: 'nx-modal-general',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalGeneralComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(@Inject('languageService') private language: any,
                private location: Location,
                private modalService: NgbModal) {
    }

    private dialog(message, title, actionLabel, actionType, cancelLabel,
                   hasFooter, cancellable, closable, params?) {
        this.modalRef = this.modalService.open(GeneralModalContent);
        this.modalRef.componentInstance.language = this.language;

        this.modalRef.componentInstance.message = message;
        this.modalRef.componentInstance.title = title;
        this.modalRef.componentInstance.actionLabel = actionLabel;
        this.modalRef.componentInstance.buttonType = actionType;
        this.modalRef.componentInstance.cancelLabel = cancelLabel;
        this.modalRef.componentInstance.buttonClass = actionType;

        this.modalRef.componentInstance.hasFooter = hasFooter;
        this.modalRef.componentInstance.cancellable = cancellable;
        this.modalRef.componentInstance.closable = closable;

        return this.modalRef;
    }

    openConfirm(message, title, actionLabel, actionType, cancelLabel) {
        return this.dialog(message, title, actionLabel, actionType, cancelLabel,
            true,
            false,
            false);
    }

    openAlert(message, title) {
        return this.dialog(message, title,
            this.language.lang.dialogs.okButton,
            null,
            this.language.lang.dialogs.cancelButton,
            true,
            true,
            true);

    }

    close() {
        this.modalRef.close();
    }

    ngOnInit() {
    }
}
