"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const testing_1 = require("@angular/core/testing");
const testing_2 = require("@angular/http/testing");
const http_1 = require("@angular/http");
const quote_service_1 = require("./quote.service");
describe('QuoteService', () => {
    let quoteService;
    let mockBackend;
    beforeEach(() => {
        testing_1.TestBed.configureTestingModule({
            providers: [
                quote_service_1.QuoteService,
                testing_2.MockBackend,
                http_1.BaseRequestOptions,
                {
                    provide: http_1.Http,
                    useFactory: (backend, defaultOptions) => {
                        return new http_1.Http(backend, defaultOptions);
                    },
                    deps: [testing_2.MockBackend, http_1.BaseRequestOptions]
                }
            ]
        });
    });
    beforeEach(testing_1.inject([
        quote_service_1.QuoteService,
        testing_2.MockBackend
    ], (_quoteService, _mockBackend) => {
        quoteService = _quoteService;
        mockBackend = _mockBackend;
    }));
    afterEach(() => {
        mockBackend.verifyNoPendingRequests();
    });
    describe('getRandomQuote', () => {
        it('should return a random Chuck Norris quote', testing_1.fakeAsync(() => {
            // Arrange
            const mockQuote = 'a random quote';
            const response = new http_1.Response(new http_1.ResponseOptions({
                body: { value: mockQuote }
            }));
            mockBackend.connections.subscribe((connection) => connection.mockRespond(response));
            // Act
            const randomQuoteSubscription = quoteService.getRandomQuote({ category: 'toto' });
            testing_1.tick();
            // Assert
            randomQuoteSubscription.subscribe((quote) => {
                expect(quote).toEqual(mockQuote);
            });
        }));
        it('should return a string in case of error', testing_1.fakeAsync(() => {
            // Arrange
            const response = new http_1.Response(new http_1.ResponseOptions({ status: 500 }));
            mockBackend.connections.subscribe((connection) => connection.mockError(response));
            // Act
            const randomQuoteSubscription = quoteService.getRandomQuote({ category: 'toto' });
            testing_1.tick();
            // Assert
            randomQuoteSubscription.subscribe((quote) => {
                expect(typeof quote).toEqual('string');
                expect(quote).toContain('Error');
            });
        }));
    });
});
//# sourceMappingURL=quote.service.spec.js.map