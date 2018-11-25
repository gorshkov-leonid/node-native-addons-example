var hooker = require('./hooker.js');
hooker.addHook();
setTimeout(()=>hooker.removeHook(), 10000);

