import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { CamTableComponent } from './cam-table.component';

describe('CamTableComponent', () => {
  let component: CamTableComponent;
  let fixture: ComponentFixture<CamTableComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ CamTableComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(CamTableComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
