import { Component, Inject, Input, Renderer2, ViewChild, ViewEncapsulation } from '@angular/core';
import { NgbModal, NgbActiveModal, NgbModalRef }       from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector: 'nx-modal-merge-content',
    templateUrl: 'merge.component.html',
    styleUrls: ['merge.component.scss']
})
export class MergeModalContent {
    @Input() system;
    @Input() systems;
    @Input() systemName;
    @Input() language;
    @Input() closable;

    checking: boolean;
    checkMergeabilityProcess: any;
    config: any;
    masterId: string;
    mergingProcess: any;
    multipleSystems: boolean;
    outOfDate: boolean;
    password: string;
    processedSystems: any;
    state: string;
    systemError: boolean;
    systemMergeable: string;
    targetSystem: any;
    targetSystemDropdown: any;
    tooManySystems: boolean;
    user: any;
    wrongPassword: boolean;

    @ViewChild('mergeForm') mergeForm: HTMLFormElement;

    constructor(public activeModal: NgbActiveModal,
                public renderer: Renderer2,
                @Inject('process') private process: any,
                @Inject('account') private account: any,
                @Inject('system') private systemService: any,
                @Inject('configService') private configService: any,
                @Inject('systemsProvider') private systemsProvider: any,
                @Inject('cloudApiService') private cloudApi: any) {
        this.config = this.configService.config;
        this.checking = false;
        this.state = 'select';
        this.wrongPassword = false;
    }

    ngOnInit() {
        this.masterId = this.system.id;
        this.multipleSystems = this.systems.length > 0;
        this.outOfDate = this.multipleSystems && !this.system.canMerge;
        this.processedSystems = this.makeSelectorList(this.systems);
        this.targetSystem = this.selectDefaultSystem();
        this.targetSystemDropdown = this.makeSelectorList([this.targetSystem])[0];
        this.systemMergeable = this.checkMergeability(this.targetSystem);
        this.systemError = !this.multipleSystems || this.outOfDate;

        this.account.get().then((user) => {
            this.user = user;
        });

        this.mergingProcess = this.process.init(() => {
            let masterSystemId;
            let slaveSystemId;
            if (this.masterId === this.system.id) {
                masterSystemId = this.system.id;
                slaveSystemId = this.targetSystem.id;
            } else {
                masterSystemId = this.targetSystem.id;
                slaveSystemId = this.system.id;
            }
            return this.cloudApi.merge(masterSystemId, slaveSystemId, this.password);
        }, {
            errorCodes: {
                mergedSystemIsOffline: (error) => {
                    return this.language.errorCodes[error.errorText] || error.errorText;
                },
                vmsRequestFailure: (error) => {
                    const errorText = this.language.errorCodes[error.errorText] || error.errorText;
                    let errorData = '';
                    if (!(errorText in this.language.errorCodes) && error.errorData) {
                        errorData = JSON.stringify(error.errorData);
                    }
                    return errorData ? `${errorText}\nError Data: ${errorData}` : errorText;
                },
                wrongPassword: () => {
                    this.mergeForm.controls['mergePassword'].setErrors({'wrongPassword': true});
                    this.password = '';

                    this.renderer.selectRootElement('#mergePassword').focus();
                    this.wrongPassword = true;

                },
            },
            successMessage: this.language.system.mergeSystemSuccess
        }).then(() => {
            this.systemsProvider.forceUpdateSystems();
            this.activeModal.close({
                anotherSystemId: this.targetSystem.id,
                role: this.masterId === this.system.id ?
                    this.configService.config.systemStatuses.master :
                    this.configService.config.systemStatuses.slave
            });
        });

        this.checkMergeabilityProcess = this.process.init(() => {
            this.checking = true;
            this.systemMergeable = '';
            return this.precheckSystemMerge();
        }, {
            errorCodes: {}
        }).then((res) => {
            this.checking = false;
            this.systemMergeable = this.checkMergeability(this.targetSystem);
            if (!res.system && this.systemMergeable === '') {
                return this.updateState();
            }
        });
    }

    addStatus(system) {
        let status = '';

        if (system.stateOfHealth === 'offline' || system.hasOwnProperty('isOnline') && !system.isOnline) {
            status = ` - ${this.language.systemStatuses.offline}`;
        } else if (system.stateOfHealth === 'online' && !system.canMerge) {
            status = ` - ${this.language.systemStatuses.incompatible}`;
        } else if (system.stateOfHealth === 'unavailable' || system.hasOwnProperty('isAvailable') && !system.isAvailable) {
            status = ` - ${this.language.systemStatuses.unavailable}`;
        }

        return `<span>${system.name}</span><span class="text-muted">${status}</span>`;
    }

    // Add system can merge where added to systems form api call
    checkMergeability(system) {
        const stateOfHealth = system.info && system.info.stateOfHealth || system.stateOfHealth || system.stateMessage || '';

        if (system.hasOwnProperty('isOnline') && !system.isOnline || stateOfHealth.indexOf('offline') > -1) {
            return 'offline';
        }
        if (system.hasOwnProperty('isAvailable') && !system.isAvailable || stateOfHealth.indexOf('unavailable') > -1) {
            return 'unavailable';
        }
        if (!system.canMerge) {
            return 'secondaryCannotMerge';
        }
        if (!this.system.canMerge) {
            return 'primaryCannotMerge';
        }
        return '';
    }

    precheckSystemMerge() {
        return this.systemService(this.targetSystem.id, this.user.email).update().then((system) => {
            this.targetSystem = {...this.targetSystem, ...system};
            return Promise.all([
                this.system.mediaserver.getMediaServers().catch(error => {
                    return Promise.reject({system: 'primary', errorResponse: error});
                }),
                this.targetSystem.mediaserver.getMediaServers().catch(error => {
                    return Promise.reject({system: 'secondary', errorResponse: error});
                })
            ]).then(res => {
                this.tooManySystems = res.map(req => req.data.length)
                    .reduce((acc, cur) => acc + cur) > this.config.maxServers;
                return {};
            }).catch(error => error);
        });
    }

    makeSelectorList(systems) {
        return systems.map(system => {
            return { id: system.id, name: this.addStatus(system) };
        });
    }

    selectDefaultSystem() {
        if (this.systems.length < 1) {
            throw 'Error User needs to be the owner of more that 1 system';
        }
        for (const i in this.systems) {
            if (this.checkMergeability(this.systems[i]) === '') {
                return {...this.systems[i]};
            }
        }
        return {...this.systems[0]};
    }

    setTargetSystem(targetSystem) {
        this.systemMergeable = '';
        this.targetSystem = {... this.systems.find(system => system.id === targetSystem.id)};
    }

    updateState() {
        switch (this.state) {
            case 'select':
                this.state = this.tooManySystems ? 'warning' : 'confirm';
                break;
            case 'warning':
                this.state = 'confirm';
                break;
            default:
                break;
        }
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

    private dialog(system, systems) {
        this.modalRef = this.modalService.open(MergeModalContent, {backdrop: 'static', centered: true});
        this.modalRef.componentInstance.language = this.language.lang;
        this.modalRef.componentInstance.system = system;
        this.modalRef.componentInstance.systems = systems;
        this.modalRef.componentInstance.closable = true;

        return this.modalRef;
    }

    open(system, systems) {
        return this.dialog(system, systems).result;
    }

    close() {
        this.modalRef.close({});
    }
}
