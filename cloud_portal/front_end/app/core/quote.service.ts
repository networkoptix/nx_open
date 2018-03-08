import { Injectable } from '@angular/core';
import { HttpClient } from '@angular/common/http';

const routes = {
    quote: (c: RandomQuoteContext) => `https://api.chucknorris.io/jokes/random?category=${c.category}`
};

export interface RandomQuoteContext {
    // The quote's category: 'nerdy', 'explicit'...
    category: string;
}

@Injectable()
export class QuoteService {
    data: any;

    constructor(private http: HttpClient) {
    }

    getRandomQuote(context: RandomQuoteContext) {
        return this.http.get(routes.quote(context), {})
    }
}
