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
let NxProcessButtonComponent = class NxProcessButtonComponent {
    constructor() {
    }
    ngOnInit() {
        this.buttonClass = 'btn-primary';
        if (this.actionType) {
            this.buttonClass = 'btn-' + this.actionType;
        }
    }
    touchForm() {
        for (const ctrl in this.form.form.controls) {
            this.form.form.get(ctrl).markAsTouched();
        }
    }
    setFocusToInvalid() {
        console.log('ctrls:', this.form.form.controls);
        for (const ctrl in this.form.form.controls) {
            const control = this.form.form.get(ctrl);
            // console.log('CTRL:', control);
            if (control.invalid) {
                // TODO : find how to set element's focus
                // control.focused = true;
                return;
            }
        }
    }
    checkForm() {
        if (this.form && !this.form.valid) {
            //Set the form touched
            this.touchForm();
            this.setFocusToInvalid();
            return false;
        }
        else {
            this.process.run();
        }
    }
};
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], NxProcessButtonComponent.prototype, "process", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", String)
], NxProcessButtonComponent.prototype, "buttonText", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Boolean)
], NxProcessButtonComponent.prototype, "buttonDisabled", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Boolean)
], NxProcessButtonComponent.prototype, "actionType", void 0);
__decorate([
    core_1.Input(),
    __metadata("design:type", Object)
], NxProcessButtonComponent.prototype, "form", void 0);
NxProcessButtonComponent = __decorate([
    core_1.Component({
        selector: 'nx-process-button',
        templateUrl: './components/process-button/process-button.component.html',
        styleUrls: []
    }),
    __metadata("design:paramtypes", [])
], NxProcessButtonComponent);
exports.NxProcessButtonComponent = NxProcessButtonComponent;
//# sourceMappingURL=process-button.component.js.map