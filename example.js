const cluster = require('cluster');
const shm = require('./index.js');

var buf, arr;
if (cluster.isMaster) {
	buf = shm.create(4096);
	arr = shm.create(4096, shm.TypedArrayType.Float32Array);
	console.log('[Master] Typeof buf:', buf.constructor.name, 'Typeof arr:', arr.constructor.name);
	buf[0] = 1;
	arr[0] = 1.0;
	cluster.fork().on('online', function() {
		this.send({ bufKey: buf.key, arrKey: arr.key });
		var i = 0;
		setInterval(function() {
			buf[0]++;
			arr[0] /= 2;
			console.log('[Master] Set buf[0]=', buf[0], ' arr[0]=', arr ? arr[0] : null);
			i++;
			if (i == 5) {
				for (var id in cluster.workers) {
					//console.log(id);
				    //cluster.workers[id].kill();
				}
				//process.exit();
				setTimeout(function() {
					//console.log(11);
				}, 5000);
			}

//todo : good kill both

		}, 50);
	}).on('exit', () => process.exit());
} else {
	process.on('message', function(message) {
		buf = shm.get(message.bufKey);
		arr = shm.get(message.arrKey, shm.TypedArrayType.Float32Array);
		console.log('[Worker] Typeof buf:', buf.constructor.name, 'Typeof arr:', arr.constructor.name);
		var i = 0;
		setInterval(function() {
			console.log('[Worker] Get buf[0]=', buf[0], ' arr[0]=', arr ? arr[0] : null);
			i++;
			if (i == 2) {
				shm.destroy(message.arrKey, true);
				arr = null;
			}
		}, 50);
	});
}
