import { Injectable }             from '@angular/core';
import { ActivatedRoute, Router } from '@angular/router';
import { Observable }             from 'rxjs';

@Injectable({
    providedIn: 'root'
})
export class NxUriService {

    constructor(private router: Router,
                private route: ActivatedRoute) {
    }

    getURI(): Observable<any> {
        return this.route.queryParams;
    }

    updateURI(objParams) {
        interface Params {
            [key: string]: any;
        }

        const queryParams: Params = {};

        objParams.forEach((param) => {
            queryParams[param.key] = param.value;
        });

        // changes the route without moving from the current view or
        // triggering a navigation event,
        this.router.navigate(['/campage'], {
            queryParams,
            relativeTo         : this.route,
            replaceUrl         : true,
            queryParamsHandling: 'merge',
            // do not trigger navigation
            // skipLocationChange : true
        });
    }
}
