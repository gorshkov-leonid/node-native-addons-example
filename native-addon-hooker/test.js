var hooker = require('./hooker.js');
hooker.addHook(function(obj){
	console.log(obj);
}, 10376);
setTimeout(()=>hooker.removeHook(), 60000);

