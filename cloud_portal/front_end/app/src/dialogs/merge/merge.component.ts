import { Component, Inject, Input, Renderer2, ViewChild, ViewEncapsulation } from '@angular/core';
import { NgbModal, NgbActiveModal, NgbModalRef }       from '@ng-bootstrap/ng-bootstrap';

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
    latestBuildUrl: string;
    masterId: string;
    merging: any;
    multipleSystems: boolean;
    outOfDate: boolean;
    password: string;
    showMergeForm: boolean;
    state: string;
    systems: any;
    systemMergeable: string;
    systemsSelect: any;
    targetSystem: any;
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

    }

    // Add system can merge where added to systems form api call
    checkMergeAbility(system) {
        if (!system.id) {
            return undefined;
        }
        if (system.stateOfHealth === 'offline' || !system.isOnline) {
            return 'offline';
        }
        if (!system.canMerge) {
            return 'cannotMerge';
        }
        return '';
    }

    setTargetSystem(system) {
        this.systemMergeable = undefined;
        this.targetSystem = {... system};
        if (!system.id) {
            return;
        }

        return this.systemService(system.id, this.user.email).update().then((system) => {
            this.targetSystem = {...this.targetSystem, ...system};
            this.systemMergeable = this.checkMergeAbility(system);

            return Promise.all([
                this.system.mediaserver.getMediaServers(),
                this.targetSystem.mediaserver.getMediaServers()
            ]).then(res => {
                this.tooManySystems = res.map(req => req.data.length)
                    .reduce((acc, cur) => acc + cur) > this.config.maxServers;
            });
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
        this.systemsSelect = [{name: '- - - -', id: ''}];
        systems.forEach(element => {
            this.systemsSelect.push({
                name: this.addStatus(element),
                id: element.id
            });
        });
    }

    ngOnInit() {
        this.systemMergeable = undefined;
        this.systemsSelect = [{name: '- - - -', id: ''}];
        this.wrongPassword = false;
        this.masterId = this.system.id;
        this.account
            .get()
            .then((user) => {
                this.state = 'select';
                this.user = user;
                this.systems = this.systemsProvider.getMySystems(user.email, this.system.id);
                this.multipleSystems = this.systems.length > 0;
                this.outOfDate = this.multipleSystems && !this.system.canMerge;
                this.makeSelectorList(this.systems);
                this.targetSystem = {... this.systemsSelect[0]};
                this.systemMergeable = this.checkMergeAbility(this.targetSystem);
                this.showMergeForm = this.multipleSystems && !this.outOfDate;
            });

        this.merging = this.process.init(() => {
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

        this.cloudApi.getDownloads().then((res) => {
            this.latestBuildUrl = `/downloads/${res.data.buildNumber}`;
        });
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

    private dialog(system) {
        this.modalRef = this.modalService.open(MergeModalContent, {backdrop: 'static', centered: true});
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
