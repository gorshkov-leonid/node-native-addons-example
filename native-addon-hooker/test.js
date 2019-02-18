var hooker = require('./hooker.js');
hooker.addHook(function(obj){
	console.log(obj);
}, 1480);
setTimeout(()=>hooker.removeHook(), 60000);

