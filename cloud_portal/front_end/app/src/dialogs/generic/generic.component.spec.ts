import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalGenericComponent } from './generic.component';

describe('NxModalGenericComponent', () => {
  let component: NxModalGenericComponent;
  let fixture: ComponentFixture<NxModalGenericComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalGenericComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalGenericComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
