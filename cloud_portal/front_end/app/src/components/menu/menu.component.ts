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
    @Input() content: any;
    @Input() elements: any;

    private level1:any;

    constructor() {
    }

    ngOnInit() {
        this.level1 = this.content.level1;
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
