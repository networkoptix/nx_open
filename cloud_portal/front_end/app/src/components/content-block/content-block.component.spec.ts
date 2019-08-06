import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxContentBlockComponent } from './content-block.component';

describe('NxContentBlockComponent', () => {
    let component: NxContentBlockComponent;
    let fixture: ComponentFixture<NxContentBlockComponent>;

    beforeEach(async(() => {
        TestBed.configureTestingModule({
                declarations: [ NxContentBlockComponent ]
            })
            .compileComponents();
    }));

    beforeEach(() => {
        fixture = TestBed.createComponent(NxContentBlockComponent);
        component = fixture.componentInstance;
        fixture.detectChanges();
    });

    it('should create', () => {
        expect(component).toBeTruthy();
    });
});
