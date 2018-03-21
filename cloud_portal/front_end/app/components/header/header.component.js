"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
var __param = (this && this.__param) || function (paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
};
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
const common_1 = require("@angular/common");
let NxHeaderComponent = class NxHeaderComponent {
    // active = {};
    constructor(systemsProvider, account, location) {
        this.systemsProvider = systemsProvider;
        this.account = account;
        this.location = location;
        this.active = {
            register: false,
            view: false,
            settings: false
        };
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
    logout() {
        this.account.logout();
    }
    ;
    isActive(val) {
        return (this.location.path().indexOf(val) >= 0); // no match
    }
    updateActive() {
        this.active.register = this.isActive('/register');
        this.active.view = this.isActive('/view');
        // this.active.settings = $route.current.params.systemId && !this.isActive('/view');
    }
    updateActiveSystem() {
        if (!this.systems) {
            return;
        }
        this.activeSystem = this.systems.find(function (system) {
            // return $route.current.params.systemId == system.id;
        });
        if (this.singleSystem) {
            this.activeSystem = this.systems[0];
        }
    }
};
NxHeaderComponent = __decorate([
    core_1.Component({
        selector: 'nx-header',
        templateUrl: './components/header/header.component.html',
        styleUrls: []
    }),
    __param(0, core_1.Inject('systemsProvider')),
    __param(1, core_1.Inject('account')),
    __metadata("design:paramtypes", [Object, Object, common_1.Location])
], NxHeaderComponent);
exports.NxHeaderComponent = NxHeaderComponent;
//# sourceMappingURL=header.component.js.map