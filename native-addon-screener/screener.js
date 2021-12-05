const screener = require('bindings')('screener');

/**
 * @typedef ScreenerRect
 * @type {object}
 * @property {number} x - leftmost x
 * @property {number} y - topmost y
 * @property {number} width - width from x to the right
 * @property {number} height - height from y to the bottom
 */

/**
 * @typedef ScreenerOptions
 * @type {object}
 * @property {string} [pathToSave] - path of screenshot to be saved.
 * @property {ScreenerRect} rect - rectangle to be used as clip area
 */

/**
 * This function takes screenshot from a screen.
 * @param {ScreenerOptions} options - Options
 * @returns {Uint8Array} bytes in PNG format
 */
module.exports.screenshot = screener.screenshot;


