## Build native extensions
* npm install --global --production windows-build-tools

> If you have problem,  try to run it manually from C:\Users\\{user}\\.windows-build-tools
* npm install -g node-gyp
* npm install

## Support in Intellij Clion
* `node-gyp configure -- -f cmake`
* `node-gyp configure --debug`
* Edit `build/Debug/CMakeLists.txt` and  `build/Release/CMakeLists.txt`
    * Replace `\\` and `\` with `/`
    * Add `../../prtscn_windows.cpp` to `screener__cxx_srcs` list
    * Add `set(CMAKE_CXX_STANDARD 17)`
* Right click on `build/Debug/CMakeLists.txt` or  `build/Release/CMakeLists.txt` and choose `Load CMake Project`
* Copy and save these files in other place because `npm install` clears them


# trash 
https://docs.google.com/document/d/10gmf1VVaLqrseLXBTBbRJ5P9o-OnjM0CCTcN9LGYSZE/edit#heading=h.83jsfybxcnut
