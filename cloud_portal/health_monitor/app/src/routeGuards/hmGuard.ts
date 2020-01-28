import { Injectable }                from '@angular/core';
import {
    ActivatedRouteSnapshot,
    CanActivate,
    Router,
    RouterStateSnapshot,
    UrlTree
}                                    from '@angular/router';
import { Observable }                from 'rxjs';
import { NxAccountService }          from '../services/account.service';
import { NxConfigService }           from '../services/nx-config';
import { NxSystem, NxSystemService } from '../services/system.service';


@Injectable()
export class HMGuard implements CanActivate {
    CONFIG: any;
    system: NxSystem;

    constructor(private router: Router,
                private configService: NxConfigService,
                private accountService: NxAccountService,
                private systemService: NxSystemService,
    ) {
        this.CONFIG = this.configService.getConfig();
    }

    canActivate(route: ActivatedRouteSnapshot,
                state: RouterStateSnapshot
    ): Observable<boolean | UrlTree> | Promise<boolean | UrlTree> | boolean | UrlTree {

        const systemId = route.pathFromRoot.find((snapshot: any) => {
            return snapshot.params.systemId;
        }).params.systemId;

        return this.accountService
            .get()
            .then(account => {
                if (account) {
                    this.system = this.systemService.createSystem(account.email, systemId);
                    return this.system
                        .getInfoAndPermissions()
                        .then((system) => {
                            const canView = system.canViewInfo();
                            if (!canView) {
                                return this.router.navigate([`/systems/${systemId}`]);
                            }
                            return canView;
                        }, _ => {
                            return this.router.navigate([`/systems/${systemId}`]);
                        });

                }
            });
    }
}
