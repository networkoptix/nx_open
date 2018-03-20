import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import {NxProcessButtonComponent } from './process-button.component';

describe('ProcessButtonComponent', () => {
  let component: NxProcessButtonComponent;
  let fixture: ComponentFixture<NxProcessButtonComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxProcessButtonComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxProcessButtonComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
