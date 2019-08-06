import { SlaPipe } from './sla.pipe';

describe('SlaPipe', () => {
    it('create an instance', () => {
        const pipe = new SlaPipe();
        expect(pipe).toBeTruthy();
    });
});
