import { Component, OnInit, Input, ChangeDetectorRef, Inject } from '@angular/core';
import { NgbDropdownModule }                                   from '@ng-bootstrap/ng-bootstrap';

@Component({
    selector: 'nx-account-settings-select',
    templateUrl: './dropdown/account-settings/account-settings.component.html',
    styleUrls: ['./dropdown/account-settings/account-settings.component.scss']
})

export class NxAccountSettingsDropdown implements OnInit {
    @Input() account;

    constructor() {
    }

    ngOnInit(): void {
        // prevent *ngIf complaining of undefined property --TT
        if (undefined === this.account) {
            this.account = {
                is_staff: false
            }
        }
    }
}
