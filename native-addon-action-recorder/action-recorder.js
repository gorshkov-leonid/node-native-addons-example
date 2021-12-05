const screener = require('screener');
const hooker = require('hooker');
const pngjs = require('pngjs');
const buffer = require('buffer');
const stream = require('stream');

const PNG = pngjs.PNG;
const Buffer = buffer.Buffer;
const Readable = stream.Readable;

const debugColors = [0x0000ff, 0x00ff00, 0xff0000];
let debugPathCounter = 0;

/**
 * @callback screenBytesCallback
 * @param {Uint8Array} - bytes for PNG encoded screenshot
 */

/**
 * This function starts recording
 * @param {number} processId - processId to be recorded
 * @param {screenBytesCallback} screenBytesCallback - callback to receive result
 */
module.exports.start = function (processId, screenBytesCallback) {
    hooker.addHook(
        function (err) {
            console.error("Error", err);
        },
        function () {
            console.log("Completed");
        },
        // {clickX, clickY, rects: [{x,y,width,height}...]}
        // - rect[0] for control, rect[1] for main window, - rect[2] (optional) is for top window
        function (event) {
            console.log(event);
            try {
                handleEventSync(event, screenBytesCallback);
            } catch (e) {
                console.error(e);
            }

            //handleEventAsync(event, screenBytesCallback);
        },
        processId
    )
};

/**
 * This function stops recording
 */
module.exports.stop = function () {
    hooker.removeHook();
}

function handleEventSync(event, screenBytesCallback) {
    const minx = Math.min(...event.rects.map(rect => rect.x));
    const maxx = Math.max(...event.rects.map(rect => rect.x + rect.width));
    const miny = Math.min(...event.rects.map(rect => rect.y));
    const maxy = Math.max(...event.rects.map(rect => rect.y + rect.height));

    let resPng = new PNG({width: maxx - minx, height: maxy - miny, filterType: 4});

    let debugColorIndex = debugColors.length - event.rects.length % debugColors.length
    for (const rect of event.rects.reverse()) {
        const res = Buffer.from(
            screener.screenshot({
                //pathToSave: `screen_${++debugPathCounter}.png`,
                rect: rect
            })
        );
        const png = PNG.sync.read(res);
        debugBorder(png, debugColors[debugColorIndex++ % debugColors.length]);
        const deltaX = rect.x - minx;
        const deltaY = rect.y - miny;
        for (let x = 0; x < png.width; x++) {
            for (let y = 0; y < png.height; y++) {
                const destX = deltaX + x;
                const destY = deltaY + y;
                resPng.data[(destY * resPng.width + destX) * 4] = png.data[(y * png.width + x) * 4];
                resPng.data[(destY * resPng.width + destX) * 4 + 1] = png.data[(y * png.width + x) * 4 + 1];
                resPng.data[(destY * resPng.width + destX) * 4 + 2] = png.data[(y * png.width + x) * 4 + 2];
                resPng.data[(destY * resPng.width + destX) * 4 + 3] = 255;
            }
        }
    }
    resPng = resPng.pack()
    const bufs = [];
    resPng.on('data', (d) => bufs.push(d));
    resPng.on('end', () => screenBytesCallback(Buffer.concat(bufs)))
}

//
// function handleEventAsync(event, screenBytesCallback) {
//     const minx = Math.min(...event.rects.map(rect => rect.x));
//     const maxx = Math.max(...event.rects.map(rect => rect.x + rect.width));
//     const miny = Math.min(...event.rects.map(rect => rect.y));
//     const maxy = Math.max(...event.rects.map(rect => rect.y + rect.height));
//
//     let resPng = new PNG({width: maxx - minx, height: maxy - miny});
//     let promise = Promise.resolve();
//     let debugColorIndex = debugColors.length - event.rects.length % debugColors.length
//     for (let rect of event.rects.reverse()) {
//         promise = promise.then(() =>
//             new Promise((resolve, reject) => {
//                 const res = Buffer.from(
//                     screener.screenshot({
//                         //pathToSave: `screen_${++debugPathCounter}.png`,
//                         rect: rect
//                     })
//                 );
//                 Readable.from(Buffer.from(res))
//                     .pipe(new PNG())
//                     .on("parsed", function () {
//                         debugBorder(this, debugColors[debugColors[debugColorIndex++ % debugColors.length]]);
//                         this.bitblt(resPng, 0, 0, this.width, this.height, rect.x - minx, rect.y - miny);
//                         resolve(this);
//                     })
//                     .on("error", reject)
//             })
//         )
//     }
// }

function debugBorder(png, color) {
    const r = (color >> 16) & 0xFF;
    const g = (color >> 8) & 0xFF;
    const b = color & 0xFF;
    if (png.width > 2) {
        for (let x of [0, 1, png.width - 2, png.width - 1]) {
            for (let y = 0; y < png.height; y++) {
                png.data[(y * png.width + x) * 4] = r;
                png.data[(y * png.width + x) * 4 + 1] = g;
                png.data[(y * png.width + x) * 4 + 2] = b;
            }
        }
    }
    if (png.height > 2) {
        for (let y of [0, 1, png.height - 2, png.height - 1]) {
            for (let x = 0; x < png.width; x++) {
                png.data[(y * png.width + x) * 4] = r;
                png.data[(y * png.width + x) * 4 + 1] = g;
                png.data[(y * png.width + x) * 4 + 2] = b;
            }
        }
    }
}


