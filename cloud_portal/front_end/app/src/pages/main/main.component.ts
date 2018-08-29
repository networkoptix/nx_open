import { Component, OnInit, OnDestroy } from '@angular/core';

@Component({
    selector: 'main-component',
    templateUrl: 'main.component.html',
    styleUrls: ['main.component.scss']
})

export class NxMainComponent implements OnInit, OnDestroy {

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

