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
const http_1 = require("@angular/common/http");
const routes = {
    quote: (c) => `https://api.chucknorris.io/jokes/random?category=${c.category}`
};
let QuoteService = class QuoteService {
    constructor(http) {
        this.http = http;
    }
    getRandomQuote(context) {
        return this.http.get(routes.quote(context), {});
    }
};
QuoteService = __decorate([
    core_1.Injectable(),
    __metadata("design:paramtypes", [http_1.HttpClient])
], QuoteService);
exports.QuoteService = QuoteService;
//# sourceMappingURL=quote.service.js.map