import { TestBed }                     from '@angular/core/testing';
import { NxIntegrationsListComponent } from './list.component';

describe('NxUsersDetailComponent', () => {

    beforeEach(() => {
        TestBed.configureTestingModule({
            declarations: [ NxIntegrationsListComponent ],
            providers   : [ {
                provide: 'barService', useValue: {
                    getData: () => {
                    }
                }
            } ]
        });
    });

    it('should work', () => {
        const fixture = TestBed.createComponent(NxIntegrationsListComponent);
        expect(fixture.componentInstance instanceof NxIntegrationsListComponent).toBe(true, 'should create NxIntegrationsListComponent');
    });
});

