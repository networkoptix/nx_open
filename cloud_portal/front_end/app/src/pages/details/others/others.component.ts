import { Component, OnInit, OnDestroy } from '@angular/core';

@Component({
    selector: 'others-detail-component',
    templateUrl: 'others.component.html',
    styleUrls: ['others.component.scss']
})

export class NxOtherDetailsComponent implements OnInit, OnDestroy {

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

