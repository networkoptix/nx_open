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

    constructor(@Inject('account') private account: any) {
    }

    ngOnInit(): void {

        this.account
            .get()
            .then(result => {
                this.settings.email = result.email;
                this.settings.is_staff = result.is_staff;
            })
    }

    logout() {
        this.account.logout();
    }
}
