IPC shared memory for NodeJs. Use as Buffer or TypedArray.
![version](https://img.shields.io/npm/v/shm-typed-array.svg)

# Install
``` bash
$ npm install shm-typed-array
$ npm test
```
Manual build:
``` bash
node-gyp configure
node-gyp build
node example.js
```
Tested on Ubuntu 16, Node v6.9.1

# API

<h4>shm.create(size, type)</h4>
Create shared memory segment.<br>
Size in bytes, type - shm.SHMBT_*, see shm.BufferType.<br>
Returns shared memory Buffer or descendant of TypedArray object, class depends on param "type"<br>
Return object has property 'key' - integer key of created shared memory segment, to use in shm.get().

<h4>shm.get(key, type)</h4>
Get created shared memory segment.

<h4>shm.detach(key)</h4>
Detach shared memory segment.<br>
If there are no other attaches for this segment, it will be destroyed.

<h4>shm.detachAll()</h4>
Detach all shared memory segments.<br>
Will be automatically called on process exit/termination.

<h4>shm.BufferType</h4>
Types of shared memory object.<br>
One of values should be passed to create() and get()
<pre>
{
	'Buffer': shm.SHMBT_BUFFER,
	'Int8Array': shm.SHMBT_INT8,
	'Uint8Array': shm.SHMBT_UINT8,
	'Uint8ClampedArray': shm.SHMBT_UINT8CLAMPED,
	'Int16Array': shm.SHMBT_INT16,
	'Uint16Array': shm.SHMBT_UINT16,
	'Int32Array': shm.SHMBT_INT32,
	'Uint32Array': shm.SHMBT_UINT32,
	'Float32Array': shm.SHMBT_FLOAT32, 
	'Float64Array': shm.SHMBT_FLOAT64,
};
</pre>

<h4>shm.SizeMax</h4>
Max size of shared memory segment in bytes<br>
2GB (0x7fffffff) for 64bit, 1GB for 32bit

# Usage
See example.js

``` js
const cluster = require('cluster');
const shm = require('./index.js');

var buf, arr;
if (cluster.isMaster) {
	buf = shm.create(4096); //4KB
	arr = shm.create(1000000*100*4, shm.BufferType.Float32Array); //100M floats
	buf[0] = 1;
	arr[0] = 10.0;
	console.log('[Master] Typeof buf:', buf.constructor.name, 
		'Typeof arr:', arr.constructor.name);
	
	var worker = cluster.fork();
	worker.on('online', function() {
		this.send({ msg: 'shm', bufKey: buf.key, arrKey: arr.key });
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
			arr = shm.get(data.arrKey, shm.BufferType.Float32Array);
			console.log('[Worker] Typeof buf:', buf.constructor.name, 
				'Typeof arr:', arr.constructor.name);
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
```
<b>Output:</b>
<pre>
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
shm segments destroyed: 2
</pre>
