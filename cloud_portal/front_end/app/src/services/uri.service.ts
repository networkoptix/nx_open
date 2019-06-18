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

    updateURI(navigateTo: string, objParams: any = {}, formattedParams?) {
        interface Params {
            [key: string]: any;
        }

        let queryParams: Params = {};

        if (!formattedParams) {
            objParams.forEach((param) => {
                queryParams[param.key] = param.value;
            });
        } else {
            queryParams = objParams;
        }

        // changes the route without moving from the current view or
        // triggering a navigation event,
        this.router.navigate([navigateTo], {
            queryParams,
            relativeTo         : this.route,
            replaceUrl         : true,
            queryParamsHandling: 'merge',
            // do not trigger navigation
            // skipLocationChange : true
        });
    }
}
