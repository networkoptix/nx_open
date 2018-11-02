import { TestBed }                       from '@angular/core/testing';
import { NxIntegrationDetailsComponent } from './details.component';

describe('NxIntegrationDetailsComponent', () => {

    beforeEach(() => {
        TestBed.configureTestingModule({
            declarations: [NxIntegrationDetailsComponent ],
            providers   : [ {
                provide: 'barService', useValue: {
                    getData: () => {
                    }
                }
            } ]
        });
    });

    it('should work', () => {
        const fixture = TestBed.createComponent(NxIntegrationDetailsComponent);
        expect(fixture.componentInstance instanceof NxIntegrationDetailsComponent)
                .toBe(true, 'should create NxIntegrationDetailsComponent');
    });
});

