var hooker = require('./hooker.js');

hooker.addHook(
    //error
    function (err) {
        console.error("Error", err);
    },
    //ok
    function () {
        console.log("Completed");
    },
    //progress
    /**
     * @param event {clickX, clickY, rects: [{x,y,width,height}...]}
     */
    function (event) {
        console.log(event);
    },
    8240
)

setTimeout(() => hooker.removeHook(), 15000);


