import { Component, OnInit } from '@angular/core';

@Component({
    selector   : 'right-menu-component',
    templateUrl: 'right-menu.component.html',
    styleUrls  : [ 'right-menu.component.scss' ]
})

export class NxRightMenuComponent implements OnInit {

    private content: any;

    private setupDefaults() {
        this.content = {
            main    : 'Main content',
            sections: [
                { header: 'section1', content: '<table></table>' },
                { header: 'section2', content: 'Other content' }
            ]
        };
    }

    constructor() {
        this.setupDefaults();
    }

    ngOnInit(): void {

    }
}

