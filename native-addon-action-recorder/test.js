const fs = require('fs');
const actionRecorder = require('./action-recorder.js');

let i = 0;

actionRecorder.start(12400, (bytes) => {
    fs.writeFileSync(`screen_x_${++i}.png`, bytes);
})

setTimeout(() => actionRecorder.stop(), 60000);
