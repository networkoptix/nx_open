import { TestBed, inject } from '@angular/core/testing';

import { NxRibbonService } from './ribbon.service';

describe('NxRibbonService', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [NxRibbonService]
    });
  });

  it('should be created', inject([NxRibbonService], (service: NxRibbonService) => {
    expect(service).toBeTruthy();
  }));
});
