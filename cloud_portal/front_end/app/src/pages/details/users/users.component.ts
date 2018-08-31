import { Component, OnInit, OnDestroy } from '@angular/core';

@Component({
    selector: 'users-detail-component',
    templateUrl: 'users.component.html',
    styleUrls: ['users.component.scss']
})

export class NxUsersDetailComponent implements OnInit, OnDestroy {

    private setupDefaults() {
    }

    constructor() {
        this.setupDefaults();
    }

    ngOnInit(): void {
    }

    ngOnDestroy() {
    }

    onSubmit() {
    }
}

