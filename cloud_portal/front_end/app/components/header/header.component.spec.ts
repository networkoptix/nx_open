import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import {NxHeaderComponent} from "./header.component";

describe('NxHeaderComponent', () => {
  let component: NxHeaderComponent;
  let fixture: ComponentFixture<NxHeaderComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxHeaderComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxHeaderComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
