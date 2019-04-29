import { Inject, Injectable, PLATFORM_ID }       from '@angular/core';
import { ActivatedRoute, Router, NavigationEnd } from '@angular/router';
import { Observable }                            from 'rxjs';
import { isPlatformBrowser }                     from '@angular/common';


@Injectable({
    providedIn: 'root'
})
export class NxUriService {

    constructor(private router: Router,
                private route: ActivatedRoute,
                @Inject(PLATFORM_ID) private platformId: object) {
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

        // preserve window offset
        const offset = window.pageYOffset;

        // changes the route without moving from the current view or
        // triggering a navigation event,
        this.router.navigate([navigateTo], {
            queryParams,
            relativeTo         : this.route,
            replaceUrl         : false,
            queryParamsHandling: 'merge',
            // do not trigger navigation
            // skipLocationChange : true
        });

        if (isPlatformBrowser(this.platformId)) {
            this.router.events.subscribe((event: NavigationEnd) => {
                window.scroll(0, offset);
            });
        }
    }

    resetURI(navigateTo: string) {
        this.router.navigate([navigateTo], {
            queryParams: {}
        });
    }
}
