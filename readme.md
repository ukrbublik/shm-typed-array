IPC shared memory for Node.js  
Use as `Buffer` or `TypedArray`  
[![npm](https://img.shields.io/npm/v/shm-typed-array.svg)](https://www.npmjs.com/package/shm-typed-array) [![travis](https://travis-ci.org/ukrbublik/shm-typed-array.svg?branch=master)](https://travis-ci.com/github/ukrbublik/shm-typed-array)


# Install
``` bash
$ npm install shm-typed-array
```
Windows is not supported.


# API

### shm.create (count, typeKey [, key])
Create shared memory segment.  
`count` - number of elements (not bytes), 
`typeKey` - type of elements (`'Buffer'` by default, see list below), 
`key` - optional integer to create shm with specific key.  
Returns shared memory `Buffer` or descendant of `TypedArray` object, class depends on param `typeKey`.  
Or returns `null` if shm can't be created.  
Returned object has property `key` - integer key of created shared memory segment, to use in `shm.get()`.

### shm.get (key, typeKey)
Get created shared memory segment. 
Returns `null` if shm can't be opened.

### shm.detach (key)
Detach shared memory segment.  
If there are no other attaches for this segment, it will be destroyed.

### shm.detachAll ()
Detach all created shared memory segments.  
Will be automatically called on process exit, see [Cleanup](#cleanup).

#### Types:
```js
shm.BufferType = {
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
```

### shm.getTotalSize()
Get total size of all shared segments in bytes.

### shm.LengthMax
Max length of shared memory segment (count of elements, not bytes)  
2^31 for 64bit, 2^30 for 32bit


# Cleanup
This library does cleanup of created SHM segments only on normal exit of process, see [`exit` event](https://nodejs.org/api/process.html#process_event_exit).  
If you want to do cleanup on terminate signals like `SIGINT`, `SIGTERM`, please use [node-cleanup](https://github.com/jtlapp/node-cleanup) / [node-death](https://github.com/jprichardson/node-death) and add code to exit handlers:
```js
shm.detachAll();
```


# Usage
See [example.js](https://github.com/ukrbublik/shm-typed-array/blob/master/test/example.js)

``` js
const cluster = require('cluster');
const shm = require('./index.js');

var buf, arr;
if (cluster.isMaster) {
	buf = shm.create(4096); //4KB
	arr = shm.create(1000000*100, 'Float32Array'); //100M floats
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
			arr = shm.get(data.arrKey, 'Float32Array');
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

**Output:**
```
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
```
