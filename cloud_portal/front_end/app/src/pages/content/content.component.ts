import { Component, OnInit } from '@angular/core';

@Component({
    selector   : 'content-component',
    templateUrl: 'content.component.html',
    styleUrls  : [ 'content.component.scss' ]
})

export class NxContentComponent implements OnInit {

    private allElements: any;
    private elements: any;

    private setupDefaults() {
        this.allElements = [];
        this.elements = [];
    }

    constructor() {
        this.setupDefaults();
    }

    ngOnInit(): void {

    }
}

