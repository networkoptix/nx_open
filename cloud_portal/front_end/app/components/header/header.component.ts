import {Component, OnInit, Input, Output, EventEmitter, Inject} from '@angular/core';
import {Location} from '@angular/common';

@Component({
    selector: 'nx-header',
    templateUrl: './components/header/header.component.html',
    styleUrls: []
})
export class NxHeaderComponent implements OnInit {
    config: any;
    active = {
        register: false,
        view: false,
        settings: false
    };
    systems: any;
    activeSystem: any;
    singleSystem: any;

    // active = {};

    constructor(@Inject('systemsProvider') private systemsProvider: any,
                @Inject('account') private account: any,
                private location: Location
                ) {
    }

    ngOnInit() {
        // this.config = CONFIG;
        this.updateActive();

        // inline = typeof(this.location.search().inline) != 'undefined';

        // if(this.inline) {
        //     $('body').addClass('inline-portal');
        // }

        // this.account.get().then(function (account) {
        //         this.account = account;
        //
        //     // $('body').removeClass('loading');
        //     // $('body').addClass('authorized');
        // }, function () {
        //     // $('body').removeClass('loading');
        //     // $('body').addClass('anonymous');
        // });
    }

    login() {
        alert('LOGIN!');
    }

    logout () {
        this.account.logout();
    };

    private isActive(val) {
        return (this.location.path().indexOf(val) >= 0); // no match
    }

    private updateActive() {
        this.active.register = this.isActive('/register');
            this.active.view = this.isActive('/view');
            // this.active.settings = $route.current.params.systemId && !this.isActive('/view');
    }

    private updateActiveSystem(): void {
        if (!this.systems) {
            return;
        }
        this.activeSystem = this.systems.find(function (system) {
            // return $route.current.params.systemId == system.id;
        });
        if (this.singleSystem) { // Special case for a single system - it always active
            this.activeSystem = this.systems[0];
        }
    }

    // scope.$watch('systemsProvider.systems', function () {
    //     scope.systems = scope.systemsProvider.systems;
    //     scope.singleSystem = scope.systems.length == 1;
    //     scope.systemCounter = scope.systems.length;
    //     updateActiveSystem();
    // });

    // scope.$on('$locationChangeSuccess', function (next, current) {
    //     if ($route.current.params.systemId) {
    //         if (!scope.systems) {
    //             scope.systemsProvider.forceUpdateSystems();
    //         } else {
    //             updateActiveSystem();
    //         }
    //     }
    //     updateActive();
    // });

}
