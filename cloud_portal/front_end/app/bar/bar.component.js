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
const index_1 = require("../../app/core/index");
let BarComponent = class BarComponent {
    constructor(quoteService, changeDetector) {
        this.quoteService = quoteService;
        this.changeDetector = changeDetector;
        this.title = 'bar';
    }
    ngOnInit() {
        this.quoteService
            .getRandomQuote({ category: 'dev' })
            .subscribe((data) => {
            this.message = data.value;
            // this.changeDetector.detectChanges();
        }, (err) => {
            console.log(err.error);
            console.log(err.name);
            console.log(err.message);
            console.log(err.status);
        });
    }
};
BarComponent = __decorate([
    core_1.Component({
        selector: 'bar-component',
        templateUrl: 'bar.component.html'
    }),
    __metadata("design:paramtypes", [index_1.QuoteService, core_1.ChangeDetectorRef])
], BarComponent);
exports.BarComponent = BarComponent;
//# sourceMappingURL=bar.component.js.map