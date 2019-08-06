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
            level1: [
                {
                    id: 'servers',
                    label: 'Servers',
                    path: 'servers',
                    level2: [
                        {
                            id: '12345',
                            label: 'Good server',
                            path: 'servers',
                        },
                        {
                            id: '67890',
                            label: 'Bad server',
                            path: 'servers',
                            level3: [
                                {
                                    id: '1234absd',
                                    label: 'Status',
                                    path: 'servers',
                                },
                                {
                                    id: '5678efgh',
                                    label: 'Settings',
                                    path: 'servers',
                                }
                            ]
                        }
                    ]
                },
                {
                    id: 'users',
                    label: 'Users',
                    path: 'users',
                    level3: [
                        {
                            id: 'absd',
                            label: 'Tsanko',
                            path: 'users',
                        },
                        {
                            id: 'efgh',
                            label: 'Evgeny',
                            path: 'users',
                        }
                    ]
                },
                {
                    id: 'monu-layout',
                    label: 'Menu layout',
                    path: 'other',
                    target: 'secondary'
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

