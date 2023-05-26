const cluster = require('cluster');
const shm = require('../index.js');
const assert = require('assert');

const key1 = 12345678;
const unexistingKey = 1234567891;
const posixKey = '/1234567';

let buf, arr;
if (cluster.isMaster) {
	cleanup();

	// Assert that creating shm with same key twice will fail
	const a = shm.create(10, 'Float32Array', key1); //SYSV
	const b = shm.create(10, 'Float32Array', key1); //SYSV
	assert.equal(shm.getTotalCreatedSize(), 10*4);
	assert.equal(shm.getTotalSize(), 10*4);
	assert(a instanceof Float32Array);
	assert.equal(a.key, key1);
	assert.equal(b, null);

	// Detach and destroy
	let attachesCnt = shm.detach(a.key);
	assert.equal(attachesCnt, 0);
	assert.equal(shm.getTotalSize(), 0);
	assert.equal(shm.getTotalCreatedSize(), 0);

	// Assert that getting shm by unexisting key will fail
	const c = shm.get(unexistingKey, 'Buffer');
	assert(c === null);

	// Test using shm between 2 node processes
	buf = shm.create(4096); //4KB, SYSV
	assert.equal(shm.getTotalSize(), 4096);
	arr = shm.create(10000, 'Float32Array', posixKey); //1M floats, POSIX
	assert.equal(shm.getTotalSize(), 4096 + 10000*4+8); // extra 8 bytes for size of POSIX buffer (for 64bit system)
	assert.equal(shm.getTotalCreatedSize(), 4096 + 10000*4+8);
	assert(arr && typeof arr.key === 'undefined');
	//bigarr = shm.create(1000*1000*1000*1.5, 'Float32Array'); //6Gb
	assert.equal(arr.length, 10000);
	assert.equal(arr.byteLength, 4*10000);
	buf[0] = 1;
	arr[0] = 10.0;
	//bigarr[bigarr.length-1] = 6.66;
	console.log('[Master] Typeof buf:', buf.constructor.name,
			'Typeof arr:', arr.constructor.name);

	const worker = cluster.fork();
	worker.on('online', function() {
		this.send({
			msg: 'shm',
			bufKey: buf.key,
			arrKey: posixKey, //arr.key,
			//bigarrKey: bigarr.key,
		});
		let i = 0;
		setInterval(function() {
			buf[0] += 1;
			arr[0] /= 2;
			console.log(i + ' [Master] Set buf[0]=', buf[0],
				' arr[0]=', arr ? arr[0] : null);
			i++;
			if (i == 5) {
				groupSuicide();
			}
		}, 500);
	});

	process.on('exit', cleanup);
} else {
	process.on('message', function(data) {
		const msg = data.msg;
		if (msg == 'shm') {
			buf = shm.get(data.bufKey);
			arr = shm.get(data.arrKey, 'Float32Array');
			assert.equal(shm.getTotalCreatedSize(), 0);
			// actual size of POSIX object can be multiple of PAGE_SIZE = 4096, but not for all OS
			assert(shm.getTotalSize() == 4096 + 40960 || shm.getTotalSize() == 4096 + 10000*4+8);

			//bigarr = shm.get(data.bigarrKey, 'Float32Array');
			console.log('[Worker] Typeof buf:', buf.constructor.name,
					'Typeof arr:', arr.constructor.name);
			//console.log('[Worker] Test bigarr: ', bigarr[bigarr.length-1]);
			let i = 0;
			setInterval(function() {
				console.log(i + ' [Worker] Get buf[0]=', buf[0],
					' arr[0]=', arr ? arr[0] : null);
				i++;
				if (i == 2) {
					shm.detach(data.arrKey);
					arr = null; //otherwise process will drop
				}
			}, 500);
		} else if (msg == 'exit') {
			process.exit();
		}
	});
}

function cleanup() {
	try {
		if (shm.destroy(key1)) {
			console.log(`Destroyed System V shared memory segment with key ${key1}`);
		}
	} catch(_e) {}
	try {
		if (shm.destroy(posixKey)) {
			console.log(`Destroyed POSIX shared memory object with name ${posixKey}`);
		}
	} catch(_e) {}
	assert.equal(shm.getTotalSize(), 0);
};

function groupSuicide() {
	if (cluster.isMaster) {
		for (const id in cluster.workers) {
		    cluster.workers[id].send({ msg: 'exit'});
		    cluster.workers[id].destroy();
		}
		process.exit();
	}
}

/**
 * Output
 *

[Master] Typeof buf: Buffer Typeof arr: Float32Array
[Worker] Typeof buf: Buffer Typeof arr: Float32Array
0 [Master] Set buf[0]= 2  arr[0]= 5
0 [Worker] Get buf[0]= 2  arr[0]= 5
1 [Master] Set buf[0]= 3  arr[0]= 2.5
1 [Worker] Get buf[0]= 3  arr[0]= 2.5
2 [Master] Set buf[0]= 4  arr[0]= 1.25
2 [Worker] Get buf[0]= 4  arr[0]= null
3 [Master] Set buf[0]= 5  arr[0]= 0.625
3 [Worker] Get buf[0]= 5  arr[0]= null
4 [Master] Set buf[0]= 6  arr[0]= 0.3125
shm segments destroyed: 1
Destroyed POSIX memory object with key /1234567

*/
