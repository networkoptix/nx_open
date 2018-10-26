import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalMergeComponent } from './merge.component';

describe('NxModalMergeComponent', () => {
  let component: NxModalMergeComponent;
  let fixture: ComponentFixture<NxModalMergeComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalMergeComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalMergeComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
