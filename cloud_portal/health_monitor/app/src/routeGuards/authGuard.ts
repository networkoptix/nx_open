import { Injectable } from '@angular/core';
import {
    ActivatedRouteSnapshot,
    CanActivate,
    Router,
    RouterStateSnapshot,
    UrlTree
} from '@angular/router';
import { Observable } from 'rxjs';
import { NxAccountService } from '../services/account.service';
import { NxConfigService } from '../services/nx-config';


@Injectable()
export class AuthGuard implements CanActivate {
    CONFIG: any;
    constructor(private router: Router,
                private configService: NxConfigService,
                private accountService: NxAccountService) {
        this.CONFIG = this.configService.getConfig();
    }

    canActivate(route: ActivatedRouteSnapshot,
                state: RouterStateSnapshot
    ): Observable<boolean | UrlTree> | Promise<boolean | UrlTree> | boolean | UrlTree {
        // All route to pass account service will handle auth login.
        if (state.root.queryParams.auth) {
            return true;
        }

        // check if requested in iFrame
        if (window.location !== window.parent.location) {
            return false;
        }

        return this.accountService.requireLogin().then((account) => {
            return account !== undefined;
        });
    }
}
