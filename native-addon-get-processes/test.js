var getWindowList = require('./WindowListFunctions.js');
//https://stackoverflow.com/questions/18050095/node-js-will-node-always-wait-for-settimeout-to-complete-before-exiting
setInterval(function() {
    /*{
      windowName: 'Scheduling: setTimeout and setInterval - Google Chrome',
      windowClassName: 'Chrome_WidgetWin_1',
      windowHandle: '',
      exeFullName: 'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe'
	}
	*/
	getWindowList.getWindowList(function(res){
		console.log(res);
		//console.log(res.map((r)=>r.windowName));
		//console.log(res.map((r)=>r.windowHandle+", "+Number(r.windowHandle)));
	});
}, 3000);



