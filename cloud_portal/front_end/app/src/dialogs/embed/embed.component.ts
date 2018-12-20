import {
    Component, Inject, OnInit, Input,
    ViewEncapsulation, Renderer2,
    ChangeDetectorRef, ViewChild
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

    @ViewChild('embedForm') embedForm;

    constructor(private activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private location: Location,
                @Inject('configService') private configService: any,
                @Inject(DOCUMENT) private document: any) {

        this.params = {
            authString: '',
            nocameras : false,
            noheader  : false,
            nocontrols: false,
        };

        this.auth = {
            email   : '',
            password: ''
        };
    }

    ngOnInit() {
        this.createEmbedUrl(this.params);
    }

    ngAfterViewInit() {
        this.embedForm.form.valueChanges.subscribe((changes) => {
            this.createEmbedUrl(changes);
        });
    }

    createEmbedUrl(params): void {
        // Cannot use A6 router at this moment - AJS is leading the parade
        const url = window.location.href.replace('systems', 'embed').split('?')[ 0 ];
        let uri = '';

        for (const paramsKey in params) {
            if (params.hasOwnProperty(paramsKey)) {
                // filter checkboxes in form
                if (this.params[ paramsKey ] !== undefined && !params[ paramsKey ]) {
                    uri += (uri === '') ? '?' : '&';
                    uri += (typeof params[ paramsKey ] === 'boolean') ? paramsKey : params[ paramsKey ];
                }
            }
        }

        uri += '&auth=' + btoa(params.login_email + ':' + params.login_password);

        this.embedUrl = '<iframe ' +
                            'src = "' + url + uri + '" >' +
                            'Your browser doesn\'t support iframe.' +
                        '</iframe>';
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
