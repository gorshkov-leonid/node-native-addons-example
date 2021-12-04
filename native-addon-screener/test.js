const fs = require('fs');
const screener = require('./screener.js');

const options1 = {
    // pathToSave: "xyz.png",
    // rect: {
    //     x: 0,
    //     y: 0,
    //     width: 3820 / 2,
    //     height: 2160 / 2,
    // },
   rect: {
        x: -1920 / 2,
        y: 0,
        width: 1920 / 2,
        height: 1080,
    }
}

const res = screener.screenshot(options1);
console.log(res);
fs.writeFileSync('out.png', new Buffer(res));





