import { Component, OnInit } from '@angular/core';
import { ApiService }        from './services/api.service';

@Component({
    selector: 'app-root',
    templateUrl: './app.component.html',
    styleUrls: ['./app.component.scss']
})
export class AppComponent implements OnInit {
    items: {};
    tiles: {};
    serverTime: string;

    constructor(private api: ApiService) {
    }

    ngOnInit() {
        this.api
            .getHealthStatus()
            .subscribe((data: any) => {
                    this.items = data.metrics;
                    this.serverTime = data.server_timestamp;
                },
                error => {
                    console.error('Server error.');
                });

        this.api
            .getJSON()
            .subscribe((res: any) => {
                    this.tiles = res.tiles;
                },
                error => {
                    console.error('No tile structure provided');
                });
    }
}
