const shm = require('./index');
const nodeCleanup = require('node-cleanup');

nodeCleanup(function (exitCode, signal) {
  console.log('cleaning up...');
  shm.detachAll();
});

const timeout = (ms) => new Promise(resolve => setTimeout(resolve, ms));

(async () => {
  let s = shm.create(10, 'Int32Array');
  console.log('key: ', s.key, '0x'+s.key.toString(16));
  console.log('cmd: ', `ipcs -m | grep ${'0x'+s.key.toString(16)}`);
  if (!s) return;
  s[0] = 11;
  s[1] = 22;
  console.log(s[0], s[1], s[2]);
  await timeout(1000*30);
  shm.detach(s.key);
})();
