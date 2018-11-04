module.exports.add = function(a, b){
	const cmath = require('bindings')('cmath');
    return cmath.add(a, b);
}
