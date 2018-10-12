import {
    Component, Inject, OnInit, Input,
    ViewEncapsulation, Renderer2
}                                                from '@angular/core';
import { DOCUMENT, Location }                    from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef } from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector   : 'nx-modal-embed-content',
    templateUrl: 'embed.component.html',
    styleUrls  : []
})
export class EmbedModalContent {
    @Input() systemId;
    @Input() language;
    @Input() disconnect;
    @Input() closable;

    auth: any;
    params: any;
    embedUrl: string;

    constructor(private activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private location: Location,
                @Inject('configService') private configService: any,
                @Inject(DOCUMENT) private document: any) {

        this.params = {
            nocameras : false,
            noheader  : false,
            nocontrols: false,
        };

        this.auth = {
          email: '',
          password: ''
        };
    }

    ngOnInit() {
        // Cannot use A6 router at this moment - AJS is leading the parade
        this.embedUrl = window.location.href.replace('systems', 'embed');
    }

    copyToClipboard(): boolean {
        this.renderer.selectRootElement('#embedUrl').select();

        /* Copy the text inside the text field */
        this.document.execCommand('copy');

        return false;
    }

    getEmbedUrl(): string {
        let uri = '';

        for (const paramsKey in this.params) {
            if (this.params.hasOwnProperty(paramsKey)) {
                if (!this.params[ paramsKey ]) {
                    uri += (uri === '') ? '?' : '&';
                    uri += paramsKey;
                }
            }
        }

        return this.embedUrl + uri;
    }

    close() {
        this.activeModal.close();
    }
}

@Component({
    selector     : 'nx-modal-embed',
    template     : '',
    encapsulation: ViewEncapsulation.None,
    styleUrls    : []
})

export class NxModalEmbedComponent implements OnInit {
    modalRef: NgbModalRef;

    constructor(@Inject('languageService') private language: any,
                private modalService: NgbModal) {
    }

    private dialog() {
        this.modalRef = this.modalService.open(EmbedModalContent, { backdrop: 'static', centered: true });
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(systemId) {
        return this.dialog().result;
    }

    ngOnInit() {
    }
}
