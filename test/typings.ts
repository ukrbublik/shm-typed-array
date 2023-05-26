import * as shm from '../index'

// typings:expect-error
shm.create();
// typings:expect-error
let fail1: Buffer = shm.create(1);
let pass1: shm.Shm<Buffer> | null = shm.create(1);

// typings:expect-error
let fail2: Uint32Array = shm.create(1, 'Uint32Array');
let pass2: shm.Shm<Uint32Array> | null = shm.create(1, 'Uint32Array');

let pass3: shm.Shm<Float64Array> | null = shm.create(456, 'Float64Array', 1234);
let pass4: shm.Shm<Float64Array> | null = shm.create(456, 'Float64Array', '/test');
let pass5: shm.Shm<Float64Array> | null = shm.create(456, 'Float64Array', '/test', '660');

// typings:expect-error
let fail3: Buffer = shm.get(123);
let pass6 = shm.get(123);
if (pass3) pass3.key as number;

// typings:expect-error
let fail4: Float64Array = shm.get(456, 'Float64Array');
let pass7: shm.Shm<Float64Array> | null = shm.get(456, 'Float64Array');

// typings:expect-error
shm.detach();
shm.detach(123) as number;
shm.detach(123, true) as number;
shm.detach('/test') as number;
shm.detach('/test', true) as number;

// typings:expect-error
shm.destroy();
shm.destroy(123) as boolean;
shm.destroy('/test') as boolean;

shm.detachAll() as number;
shm.getTotalSize() as number;
shm.LengthMax as number;
