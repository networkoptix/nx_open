import { Component, ElementRef, Input, OnInit, ViewChild, ViewEncapsulation } from '@angular/core';

/* Usage

*/

@Component({
    selector   : 'nx-layout',
    templateUrl: 'layout.component.html',
    encapsulation: ViewEncapsulation.None,
    styleUrls  : [ 'layout.component.scss' ],
})
export class NxLayoutComponent implements OnInit {
    @Input('type') class: string;

    constructor() {
    }

    ngOnInit() {

    }
}
