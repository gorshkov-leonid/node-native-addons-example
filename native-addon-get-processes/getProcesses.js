
const getProcesses = require('bindings')('getProcesses');
console.log(getProcesses);
module.exports.getProcessesList = getProcesses.getProcessesList;


