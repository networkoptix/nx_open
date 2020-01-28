import { Component, Input, ViewEncapsulation }         from '@angular/core';
import { Location }                                    from '@angular/common';
import { NgbActiveModal, NgbModal, NgbModalRef }       from '@ng-bootstrap/ng-bootstrap';
import { NxLanguageProviderService }                   from '../../services/nx-language-provider';
import { DomSanitizer }                                from '@angular/platform-browser';
import { NgForm }                                      from '@angular/forms';

@Component({
    selector: 'nx-modal-apply-content',
    templateUrl: 'apply.component.html',
    styleUrls: []
})
export class ApplyModalContent {
    @Input() applyFunc: any;
    @Input() discardFunc: any;
    @Input() form: NgForm;

    constructor(public activeModal: NgbActiveModal,
    ) {
    }

    apply = () => {
        if (this.form) {
            this.form.form.markAllAsTouched();
        }
        this.applyFunc.then(() => {
            this.activeModal.close('applied');
        }, () => {
            this.activeModal.close('canceled');
        });
    };

    close() {
        this.activeModal.dismiss('canceled');
    }

    discard() {
        this.discardFunc();
        this.activeModal.close('discarded');
    }
}

@Component({
    selector: 'nx-modal-apply',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalApplyComponent {
    modalRef: NgbModalRef;
    LANG: any;

    constructor(private domSanitizer: DomSanitizer,
                private location: Location,
                private modalService: NgbModal,
                private language: NxLanguageProviderService,
    ) {
        this.LANG = this.language.getTranslations();
    }

    private dialog(applyFunc, discardFunc) {
        this.modalRef = this.modalService.open(ApplyModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });
        this.modalRef.componentInstance.applyFunc = applyFunc;
        this.modalRef.componentInstance.discardFunc = discardFunc;

        return this.modalRef;
    }

    open(applyFunc, discardFunc) {
        return this.dialog(applyFunc, discardFunc)
            .result;
    }
}
