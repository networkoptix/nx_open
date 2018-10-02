import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxModalRenameComponent } from './rename.component';

describe('NxModalRenameComponent', () => {
  let component: NxModalRenameComponent;
  let fixture: ComponentFixture<NxModalRenameComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxModalRenameComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxModalRenameComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
