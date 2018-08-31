import { Component, OnInit, OnDestroy } from '@angular/core';

@Component({
    selector: 'main-component',
    templateUrl: 'main.component.html',
    styleUrls: ['main.component.scss']
})

export class NxMainComponent implements OnInit, OnDestroy {

    private content: any;
    private elements: any;

    private setupDefaults() {
        this.content = {
            'level1': [
                {
                    'id':'servers',
                    'label': 'Servers',
                    'path': 'servers'
                },
                {
                    'id': 'users',
                    'label': 'Users',
                    'path': 'users'
                },
                {
                    'id': 'other',
                    'label': 'Other',
                    'path': 'other',
                    'target': 'secondary'
                }]
        };

        this.elements = [
            'servers-static', 'users-static'
        ]
    }

    constructor() {
        this.setupDefaults();
    }

    ngOnInit(): void {
    };

    ngOnDestroy() {

    }

    onSubmit() {

    }
}

