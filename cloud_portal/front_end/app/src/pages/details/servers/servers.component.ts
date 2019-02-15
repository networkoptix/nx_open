import { Component, OnInit, OnDestroy } from '@angular/core';

@Component({
    selector: 'servers-detail-component',
    templateUrl: 'servers.component.html',
    styleUrls: ['servers.component.scss']
})

export class NxServersDetailComponent implements OnInit, OnDestroy {

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

