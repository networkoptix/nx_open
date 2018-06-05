import { Input, Component, OnInit } from '@angular/core';
import * as moment from 'moment';

@Component({
    selector: 'app-info-tile',
    templateUrl: './info-tile.component.html',
    styleUrls: ['./info-tile.component.scss']
})
export class InfoTileComponent implements OnInit {
    @Input() items;

    constructor() {
    }

    ngOnInit() {
    }

    getClassFor(item) {
        return (item) ? 'healthy' : 'error';
    }
}
