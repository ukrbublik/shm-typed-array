const cluster = require('cluster');
const shm = require('../index.js');
const assert = require('assert');


var buf, arr;
if (cluster.isMaster) {
	// Assert that creating shm with same key twice will fail
	var key = 1234567890;
	var a = shm.create(10, 'Float32Array', key);
	var b = shm.create(10, 'Float32Array', key);
	assert(a instanceof Float32Array);
	assert(a.key == key);
	assert(b === null);

	// Assert that getting shm by unexisting key will fail
	var unexisting_key = 1234567891;
	var c = shm.get(unexisting_key, 'Buffer');
	assert(c === null);

	// Test using shm between 2 node processes
	buf = shm.create(4096); //4KB
	arr = shm.create(1000000, 'Float32Array'); //1M floats
	//bigarr = shm.create(1000*1000*1000*1.5, 'Float32Array'); //6Gb
	assert.equal(arr.length, 1000000);
	assert.equal(arr.byteLength, 4*1000000);
	buf[0] = 1;
	arr[0] = 10.0;
	//bigarr[bigarr.length-1] = 6.66;
	console.log('[Master] Typeof buf:', buf.constructor.name,
			'Typeof arr:', arr.constructor.name);
	
	var worker = cluster.fork();
	worker.on('online', function() {
		this.send({
			msg: 'shm',
			bufKey: buf.key,
			arrKey:
			arr.key,
			//bigarrKey: bigarr.key,
		});
		var i = 0;
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
} else {
	process.on('message', function(data) {
		var msg = data.msg;
		if (msg == 'shm') {
			buf = shm.get(data.bufKey);
			arr = shm.get(data.arrKey, 'Float32Array');
			//bigarr = shm.get(data.bigarrKey, 'Float32Array');
			console.log('[Worker] Typeof buf:', buf.constructor.name,
					'Typeof arr:', arr.constructor.name);
			//console.log('[Worker] Test bigarr: ', bigarr[bigarr.length-1]);
			var i = 0;
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

function groupSuicide() {
	if (cluster.isMaster) {
		for (var id in cluster.workers) {
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
shm segments destroyed: 3


*/
