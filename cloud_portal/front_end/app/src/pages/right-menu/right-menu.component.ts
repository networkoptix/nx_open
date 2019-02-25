import { Component, OnInit } from '@angular/core';

@Component({
    selector   : 'right-menu-component',
    templateUrl: 'right-menu.component.html',
    styleUrls  : [ 'right-menu.component.scss' ]
})

export class NxRightMenuComponent implements OnInit {

    private setupDefaults() {

    }

    constructor() {
        this.setupDefaults();
    }

    ngOnInit(): void {

    }
}

