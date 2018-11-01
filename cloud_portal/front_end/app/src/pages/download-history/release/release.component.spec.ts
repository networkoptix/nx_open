import { TestBed }          from '@angular/core/testing';
import { ReleaseComponent } from './release.component';

describe('App', () => {
    
  beforeEach(() => {
    TestBed.configureTestingModule({ 
      declarations: [ReleaseComponent],
      providers: [{ provide: 'barService', useValue: { getData: () => {} } }]
    });
  });

  it ('should work', () => {
    let fixture = TestBed.createComponent(ReleaseComponent);
    expect(fixture.componentInstance instanceof ReleaseComponent).toBe(true, 'should create ReleaseComponent');
  });
});
