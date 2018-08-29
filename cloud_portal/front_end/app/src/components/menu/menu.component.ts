import { Component, Input, OnInit } from '@angular/core';

/* Usage
<nx-menu>
</nx-menu>
*/

@Component({
    selector: 'nx-menu',
    templateUrl: 'menu.component.html',
    styleUrls: []
})
export class NxMenuComponent implements OnInit {
    @Input() type: string;

    constructor() {
    }

    ngOnInit() {

    }
}
