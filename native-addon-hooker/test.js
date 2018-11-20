var hooker = require('./hooker.js');
hooker.addHook();
setTimeout(()=>console.log("remove hook") && hooker.removeHook(), 10000);

