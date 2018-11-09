import { Component, Input, OnInit } from "@angular/core";

@Component({
    selector: 'nx-tag',
    templateUrl: 'tag.component.html',
    styleUrls: ['tag.component.scss']
})
export class NxTagComponent implements OnInit {
    @Input('large') isLarge: boolean;
    @Input('selected') selected: boolean;
    @Input('styled') isStyled: boolean;
    constructor(){}

    ngOnInit(){}
}
