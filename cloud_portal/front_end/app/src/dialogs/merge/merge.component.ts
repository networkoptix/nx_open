import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { Location }                                            from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }               from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                      from '@angular/forms';
import { NxConfigService }                                     from '../../services/nx-config';

@Component({
    selector: 'nx-modal-merge-content',
    templateUrl: 'merge.component.html',
    styleUrls: ['merge.component.scss']
})
export class MergeModalContent {
    @Input() system;
    @Input() systemName;
    @Input() language;
    @Input() closable;

    config: any;
    merging: any;
    user: any;
    systems: any;
    showMergeForm: any;
    targetSystem: any;
    systemMergeable: string;
    masterId: string;
    systemsSelect: any;

    constructor(public activeModal: NgbActiveModal,
                private configService: NxConfigService,
                @Inject('process') private process: any,
                @Inject('account') private account: any,
                @Inject('system') private systemService: any,
                @Inject('systemsProvider') private systemsProvider: any,
                @Inject('cloudApiService') private cloudApi: any) {

        this.config = configService.getConfig();
    }

    // Add system can merge where added to systems form api call
    checkMergeAbility(system) {
        if (system.stateOfHealth === 'offline' || !system.isOnline) {
            return 'offline';
        }
        if (!system.canMerge) {
            return 'cannotMerge';
        }
        return '';
    }

    setTargetSystem(system) {
        this.targetSystem = system;
        return this.systemService(system.id, this.user.email).update().then((system) => {
            this.systemMergeable = this.checkMergeAbility(system);
        });
    }

    addStatus(system) {
        let status = '';

        if (system.stateOfHealth === 'offline') {
            status = ' (offline)';
        } else if (system.stateOfHealth === 'online' && system.canMerge) {
            status = ' (incompatable)';
        }

        return system.name + status;
    }

    makeSelectorList(systems) {
        this.systemsSelect = [];
        systems.forEach(element => {
            this.systemsSelect.push({
                name: this.addStatus(element),
                id: element.id
            });
        });
    }

    ngOnInit() {
        this.systemsSelect = [];
        this.masterId = this.system.id;
        this.account
            .get()
            .then((user) => {
                this.user = user;
                this.systems = this.systemsProvider.getMySystems(user.email, this.system.id);
                this.showMergeForm = this.system.canMerge && this.systems.length > 0;
                this.makeSelectorList(this.systems);
                this.targetSystem = this.systemsSelect[0];
                return this.systemService(this.targetSystem.id, user.email).update();
            }).then(system => {
                this.systemMergeable = this.checkMergeAbility(system);
            });

        this.merging = this.process.init(() => {
            let masterSystemId = null;
            let slaveSystemId = null;
            if (this.masterId === this.system.id) {
                masterSystemId = this.system.id;
                slaveSystemId = this.targetSystem.id;
            } else {
                masterSystemId = this.targetSystem.id;
                slaveSystemId = this.system.id;
            }
            return this.cloudApi.merge(masterSystemId, slaveSystemId);
        }, {
            errorCodes: {
                mergedSystemIsOffline: (error) => {
                    return this.language.errorCodes[error.errorText] || error.errorText;
                },
                vmsRequestFailure: (error) => {
                    return this.language.errorCodes[error.errorText] || error.errorText;
                }
            },
            successMessage: this.language.system.mergeSystemSuccess
        }).then(() => {
            this.systemsProvider.forceUpdateSystems();
            this.activeModal.close({
                anotherSystemId: this.targetSystem.id,
                role: this.masterId === this.system.id ?
                    this.config.systemStatuses.master :
                    this.config.systemStatuses.slave
            });
        });
    }
}

@Component({
    selector: 'nx-modal-merge',
    template: '',
    encapsulation: ViewEncapsulation.None,
    styleUrls: []
})

export class NxModalMergeComponent {
    modalRef: NgbModalRef;

    constructor(@Inject('languageService') private language: any,
                private modalService: NgbModal) {
    }

    private dialog(system) {
        // TODO: Refactor dialog to use generic dialog
        // TODO: retire loading ModalContent (CLOUD-2493)
        this.modalRef = this.modalService.open(MergeModalContent,
                {
                            windowClass: 'modal-holder',
                            backdrop: 'static'
                        });
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(system) {
        return this.dialog(system).result;
    }

    close() {
        this.modalRef.close({});
    }
}
