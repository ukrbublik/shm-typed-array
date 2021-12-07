const shm = require('./index');
const nodeCleanup = require('node-cleanup');

const timeout = (ms) => new Promise(resolve => setTimeout(resolve, ms));

nodeCleanup(function (exitCode, signal) {
  console.log('cleaning up...');
  shm.detachAll();
});

(async () => {
  const key = 0; // put key from master here
  let s = shm.get(key, 'Int32Array');
  if (!s) return;
  console.log(s[0], s[1], s[2]);
  await timeout(1000*30);
  shm.detach(s.key);
})();
