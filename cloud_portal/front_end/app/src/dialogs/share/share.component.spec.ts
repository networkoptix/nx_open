import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalShareComponent } from './share.component';

describe('NxModalShareComponent', () => {
  let component: NxModalShareComponent;
  let fixture: ComponentFixture<NxModalShareComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalShareComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalShareComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
