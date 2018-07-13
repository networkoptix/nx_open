import { Component, Inject, OnInit, Input, ViewEncapsulation } from '@angular/core';
import { Location }                                            from '@angular/common';
import { NgbModal, NgbActiveModal, NgbModalRef }               from '@ng-bootstrap/ng-bootstrap';
import { EmailValidator }                                      from '@angular/forms';

@Component({
    selector: 'nx-modal-merge-content',
    templateUrl: 'merge.component.html',
    styleUrls: []
})
export class MergeModalContent {
    @Input() system;
    @Input() systemName;
    @Input() language;
    @Input() closable;

    merging: any;
    user: any;
    systems: any;
    showMergeForm: any;
    targetSystem: any;
    systemMergeable: string;
    masterId: string;

    constructor(public activeModal: NgbActiveModal,
                @Inject('process') private process: any,
                @Inject('account') private account: any,
                @Inject('system') private _system: any,
                @Inject('configService') private configService: any,
                @Inject('systemsProvider') private systemsProvider: any,
                @Inject('cloudApiService') private cloudApi: any) {

    }

    //Add system can merge where added to systems form api call
    checkMergeAbility(system) {
        if (system.stateOfHealth == 'offline') {
            return 'offline'
        }
        if (!system.canMerge) {
            return 'cannotMerge';
        }
        return '';
    }

    setTargetSystem(system) {
        this.targetSystem = system;
        return this._system(system.id, this.user.email).update().then((system) => {
            this.systemMergeable = this.checkMergeAbility(system);
        });
    };

    addStatus(system) {
        let status = "";

        if (system.stateOfHealth == 'offline') {
            status = ' (offline)';
        }

        else if (system.stateOfHealth == 'online' && system.canMerge) {
            status = ' (incompatable)';
        }

        return system.name + status;
    };

    ngOnInit() {
        this.masterId = this.system.id;
        this.account
            .get()
            .then((user) => {
                this.user = user;
                this.systems = this.systemsProvider.getMySystems(user.email, this.system.id);
                this.showMergeForm = this.system.canMerge && this.systems.length > 0;
                this.targetSystem = this.systems[0];
                return this._system(this.targetSystem.id, user.email).update();
            }).then((system)=>{
                this.systemMergeable = this.checkMergeAbility(system);
            });

        this.merging = this.process.init(() => {
            let masterSystemId = null;
            let slaveSystemId = null;
            if (this.masterId == this.system.id) {
                masterSystemId = this.system.id;
                slaveSystemId = this.targetSystem.id;
            }
            else {
                masterSystemId = this.targetSystem.id;
                slaveSystemId = this.system.id;
            }
            //return this.cloudApi.systems(); //In for testing purposes with merging things
            return this.cloudApi.merge(masterSystemId, slaveSystemId);
        }, {
            successMessage: this.language.system.mergeSystemSuccess
        }).then(() => {
            this.systemsProvider.forceUpdateSystems();
            this.activeModal.close({
                "anotherSystemId": this.targetSystem.id,
                "role": this.masterId == this.system.id ?
                    this.configService.systemStatuses.master :
                    this.configService.systemStatuses.slave
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

export class NxModalMergeComponent implements OnInit {
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

    ngOnInit() {
    }
}
