import { Component, Input, OnInit } from '@angular/core';

/* Usage
<nx-menu>
</nx-menu>
*/

@Component({
    selector: 'nx-menu',
    templateUrl: 'menu.component.html',
    styleUrls: ['menu.component.scss']
})
export class NxMenuComponent implements OnInit {
    @Input() content: any;
    @Input() elements: any;

    constructor() {
    }

    ngOnInit() {
    }

    // *** Breadcrumb for usage of named (auxiliary) router outlet
    // usage: [routerLink]="getItemLink(item)"
    // getItemLink(item){
    //     return [{outlets: { [item.target || 'primary'] : [item.path]}}];
    // }

    getAvailability(id) {
        return (this.elements.indexOf(id) > -1);
    }
}
