# 使用方式 | Usage
```js
const cluster=require('cluster');
const shm=require('node_shm');

var buf;
if(cluster.isMaster){
	buf=shm.create(4096);//shm.create(size)
	cluster.fork().on('online',function(){
		this.send(buf.key);//send key
		setInterval(function(){
			console.log('[Master] Set buf[0]=%d',++buf[0]);
		},500);
	}).on('exit',() => process.exit());
}else{
	process.on('message',function(message){
		buf=shm.get(message);//shm.get(key);
		setInterval(function(){
			console.log('[Worker] Get buf[0]=%d',buf[0]);
			if(buf[0]>10) process.exit();
		},500);
	});
}
```
