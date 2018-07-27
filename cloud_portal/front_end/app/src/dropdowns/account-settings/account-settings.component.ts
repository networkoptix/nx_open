import { Component, OnInit, Inject } from '@angular/core';

@Component({
    selector: 'nx-account-settings-select',
    templateUrl: 'account-settings.component.html',
    styleUrls: ['account-settings.component.scss']
})

export class NxAccountSettingsDropdown implements OnInit {
    settings = {
        email: '',
        is_staff: false
    };
    show: boolean;

    constructor(@Inject('account') private account: any) {
        this.show = false;
    }

    ngOnInit(): void {

        this.account
            .get()
            .then(result => {
                this.settings.email = result.email;
                this.settings.is_staff = result.is_staff;
            })
    }

    logout(): void {
        this.account.logout();
    }
}
