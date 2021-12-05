const hooker = require('bindings')('hooker');

/**
 * @typedef ScreenerRect
 * @type {object}
 * @property {number} x - leftmost x
 * @property {number} y - topmost y
 * @property {number} width - width from x to the right
 * @property {number} height - height from y to the bottom
 */

/**
 * @typedef HookEvent
 * @type {object}
 * @property {number} clickX - x-coordinate where click was done
 * @property {number} clickY - y-coordinate where click was done
 * @property {ScreenerRect[]} rects - areas enclosures a click. <ul>
 *    <li>rects[0] is the most narrow area found for a control.</li>
 *    <li>rects[1] is an area for main-window (todo describe diff)</li>
 *    <li>rects[2] (optional) is an area for top-window (todo describe diff)</li>
 * </ul>
 */

/**
 * @typedef HookErrorCallback
 * @type {function}
 * @property {error} - error
 */

/**
 * @typedef HookCompletionCallback
 * @type {function}
 */

/**
 * @typedef HookProgressCallback
 * @type {function}
 * @property {HookEvent} - event
 */

/**
 * This function adds a hook. Another call of this function removes prior previous hook. So the only hook can work at the time.
 * @param {HookErrorCallback} errorCallback - error callback
 * @param {HookCompletionCallback} completionCallback - completion callback
 * @param {HookProgressCallback} progressCallback - progress callback
 */
module.exports.addHook = hooker.addHook;

/**
 * This function removes the hook
 */
module.exports.removeHook = hooker.removeHook;