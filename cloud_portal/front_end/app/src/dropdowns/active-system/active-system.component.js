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
Object.defineProperty(exports, "__esModule", { value: true });
const core_1 = require("@angular/core");
const common_1 = require("@angular/common");
const router_1 = require("@angular/router");
let NxActiveSystemDropdown = class NxActiveSystemDropdown {
    constructor(location, route) {
        this.location = location;
        this.route = route;
        this.active = {
            register: false,
            view: false,
            settings: false
        };
    }
    isActive(val) {
        const currentPath = this.location.path();
        return (currentPath.indexOf(val) >= 0);
    }
    updateActive() {
        this.active.register = this.isActive('/register');
        this.active.view = this.isActive('/view');
        this.active.settings = this.routeSystemId && !this.isActive('/view');
    }
    ngOnInit() {
        // this.params = this.route.queryParams.subscribe((params: Params) => {
        //     this.routeSystemId = params['systemId'];
        //     console.log(this.route.queryParams);
        // });
        // this.activeSystem = this.system;
        this.updateActive();
    }
    ngOnDestroy() {
        // this.params.unsubscribe();
    }
    ngOnChanges(changes) {
        // this.activeSystem = this.system;
        this.updateActive();
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], NxActiveSystemDropdown.prototype, "activeSystem", void 0);
NxActiveSystemDropdown = __decorate([
    core_1.Component({
        selector: 'nx-active-system',
        templateUrl: './src/dropdowns/active-system/active-system.component.html',
        styleUrls: ['./src/dropdowns/active-system/active-system.component.scss']
    }),
    __metadata("design:paramtypes", [common_1.Location,
        router_1.ActivatedRoute])
], NxActiveSystemDropdown);
exports.NxActiveSystemDropdown = NxActiveSystemDropdown;
//# sourceMappingURL=active-system.component.js.map