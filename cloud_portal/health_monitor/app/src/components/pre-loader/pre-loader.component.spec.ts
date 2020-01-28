import { async, ComponentFixture, TestBed } from '@angular/core/testing';

import { NxPreLoaderComponent } from './pre-loader.component';

describe('NxPreLoaderComponent', () => {
  let component: NxPreLoaderComponent;
  let fixture: ComponentFixture<NxPreLoaderComponent>;

  beforeEach(async(() => {
    TestBed.configureTestingModule({
      declarations: [NxPreLoaderComponent ]
    })
    .compileComponents();
  }));

  beforeEach(() => {
    fixture = TestBed.createComponent(NxPreLoaderComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
