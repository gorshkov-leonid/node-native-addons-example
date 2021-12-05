## Build `native-addon-actionRecorder`
* npm install --global --production windows-build-tools

> If you have problem,  try to run it manually from C:\Users\\{user}\\.windows-build-tools
* npm install -g node-gyp  
* npm install 

## Support in Intellij Clion
* `node-gyp configure -- -f cmake` 
* Edit `build/Debug/CMakeLists.txt` and  `build/Release/CMakeLists.txt`
  * Replace `\\` and `\` with `/`
  * Add `../../prtscn_windows.cpp` to `screener__cxx_srcs` list  
  * Add `set(CMAKE_CXX_STANDARD 17)`
* Right click on `build/Debug/CMakeLists.txt` or  `build/Release/CMakeLists.txt` and choose `Load CMake Project`
* Copy and save these files in other place because `npm install` clears them

See also:
  * https://github.com/nodejs/gyp-next/issues/100#issuecomment-515729542


## Links todo!!!!!!!!!!!
* https://www.npmjs.com/package/screenshot-node
* https://stackoverflow.com/questions/21555394/how-to-create-bitmap-from-byte-array
* https://stackoverflow.com/questions/366768/convert-bitmap-to-png-in-memory-in-c-win32
* https://coderoad.ru/366768/%D0%9F%D1%80%D0%B5%D0%BE%D0%B1%D1%80%D0%B0%D0%B7%D0%BE%D0%B2%D0%B0%D0%BD%D0%B8%D0%B5-bitmap-%D0%B2-PNG-%D0%B2-%D0%BF%D0%B0%D0%BC%D1%8F%D1%82%D0%B8-%D0%B2-C-win32
* https://stackoverflow.com/questions/67602113/how-to-return-float32-array-from-c-in-node-addon-api
* https://riptutorial.com/cplusplus/example/1678/iterating-over-std--vector
* https://github.com/Kuzat/screenshot-node/blob/master/src/windows/prtscn_windows.cpp
* https://stackoverflow.com/questions/51383896/c-gdibitmap-to-png-image-in-memory
* https://github.com/lvandeve/lodepng alternative png encoder and decoder