import { Component, OnInit, OnDestroy } from '@angular/core';

@Component({
    selector   : 'integrations-component',
    templateUrl: 'integrations.component.html',
    styleUrls  : [ 'integrations.component.scss' ]
})

export class NxIntegrationsComponent implements OnInit, OnDestroy {

    private content: any;
    private elements: any;

    private setupDefaults() {
        this.content = {
            level1: [
                {
                    id    : 'all',
                    label : 'All integrations',
                    path  : 'integrations',
                },
                {
                    id    : 'access',
                    label : 'Access control',
                    path  : 'access',
                },
                {
                    id    : 'analytics',
                    label : 'Analytics',
                    path  : 'analytics',
                },
                {
                    id    : 'cameras',
                    label : 'Camera integrations',
                    path  : 'cameras',
                },
                {
                    id    : 'home',
                    label : 'Home automation',
                    path  : 'automation',
                },
                {
                    id    : 'psim',
                    label : 'PSIM',
                    path  : 'psim',
                }]
        };

        this.elements = [
            'servers-static', 'users-static'
        ];
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

