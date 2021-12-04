## Build `native-addon-hooker`
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
