var hooker = require('./hooker.js');

hooker.addHook(
    function (obj) {
        console.log(obj);
    },
    function (obj) {
        console.log(obj);
    },
    function (obj) {
        console.log(obj);
    },
    8240
)

setTimeout(() => hooker.removeHook(), 15000);


