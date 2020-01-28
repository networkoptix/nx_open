import { Component, Input, Renderer2 } from '@angular/core';
import { NgbActiveModal }              from '@ng-bootstrap/ng-bootstrap';
import { NxLanguageProviderService }   from '../../services/nx-language-provider';
import { NxProcessService }            from '../../services/process.service';
import { NxCloudApiService }           from '../../services/nx-cloud-api';

@Component({
    selector: 'nx-modal-rename-content',
    templateUrl: 'rename.component.html',
    styleUrls: []
})
export class RenameModalContent {
    @Input() systemId;
    @Input() systemName;
    @Input() closable;

    LANG: any;
    rename: any;

    constructor(private activeModal: NgbActiveModal,
                private renderer: Renderer2,
                private language: NxLanguageProviderService,
                private processService: NxProcessService,
                private cloudApiService: NxCloudApiService,
    ) {
        this.LANG = this.language.getTranslations();
    }

    ngOnInit() {
        this.rename = this.processService.createProcess(() => {
            return this.cloudApiService.renameSystem(this.systemId, this.systemName);
        }, {
            successMessage: this.LANG.system.successRename
        }).then(() => {
            this.activeModal.close(this.systemName);
        });
    }

    close() {
        this.activeModal.close();
    }
}