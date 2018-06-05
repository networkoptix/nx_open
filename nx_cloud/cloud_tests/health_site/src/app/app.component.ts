import { Component }  from '@angular/core';
import { ApiService } from './services/api.service';

@Component({
    selector: 'app-root',
    templateUrl: './app.component.html',
    styleUrls: ['./app.component.scss']
})
export class AppComponent {
    items: {};

    constructor(private api: ApiService) {
    }

    ngOnInit() {
        this.api
            .getHealthStatus()
            .subscribe((data: any) => {
                    this.items = data.metrics;
                },
                error => {
                    console.warn('Server error.');
                });
    }
}
