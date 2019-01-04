import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { CamViewComponent } from './cam-view.component';

describe('CamViewComponent', () => {
  let component: CamViewComponent;
  let fixture: ComponentFixture<CamViewComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [ CamViewComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(CamViewComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
