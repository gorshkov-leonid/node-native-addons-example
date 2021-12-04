#include <iostream>
#include <fstream>
#include <signal.h>
#include <Windows.h>
#include <vector>

#include <napi.h>
#include <node_api.h>
#include "prtscn_windows.cpp"

using namespace std;

void signal_callback_handler(int signum) {
    std::cout << "Termination " << signum << std::endl;
    exit(signum);
}

void saveToFile(std::vector <BYTE> &data, string path) {
    cout << "Write " << data.size() << " bytes to file " << path << "\n";
    std::ofstream fout(path, std::ios::binary);
    fout.write((char *) data.data(), data.size());
}

int32_t getIntegerField(Napi::Env &env, const Napi::Object &object, const string &fieldName, const string &fieldPath) {
    const Napi::Value &x = object.Get(fieldName);
    if (!x.IsNumber()) {
        Napi::TypeError::New(env, "Number is expected in " + fieldPath).ThrowAsJavaScriptException();
        return -1;
    }
    return x.As<Napi::Number>().Int32Value();
}

std::optional <RECT> getRect(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() == 0) {
        return {};
    }
    if (info.Length() >= 1 && !info[0].IsObject()) {
        Napi::TypeError::New(env, "Options object is expected as argument[0]").ThrowAsJavaScriptException();
        return {};
    }
    const Napi::Object &optionsJsObject = info[0].As<Napi::Object>();
    const Napi::Value &rectJsValue = optionsJsObject.Get("rect");
    if (!rectJsValue.IsObject()) {
        return {};
    }
    const Napi::Object &rectJsObj = rectJsValue.ToObject();
    int32_t x = getIntegerField(env, rectJsObj, "x", "options.rect.x");
    int32_t y = getIntegerField(env, rectJsObj, "y", "options.rect.y");
    int32_t width = getIntegerField(env, rectJsObj, "width", "options.rect.width");
    int32_t height = getIntegerField(env, rectJsObj, "height", "options.rect.height");
    return RECT{x, y, x + width, y + height};
}

std::optional <string> getFilePath(const Napi::CallbackInfo &info) {
    Napi::Env env = info.Env();
    if (info.Length() == 0) {
        return {};
    }
    if (info.Length() >= 1 && !info[0].IsObject()) {
        Napi::TypeError::New(env, "Options object is expected as argument[0]").ThrowAsJavaScriptException();
        return {};
    }
    const Napi::Object &optionsJsObject = info[0].As<Napi::Object>();
    const Napi::Value &pathToSaveJsValue = optionsJsObject.Get("pathToSave");
    if (pathToSaveJsValue.IsUndefined()) {
        return {};
    }
    return pathToSaveJsValue.As<Napi::String>();
}


Napi::Value MakeScreenshot(const Napi::CallbackInfo &info) {
    cout << "Make screenshot with options:\n";
    const optional <RECT> &rect = getRect(info);
    if (rect.has_value()) {
        cout << "  rect: {\n"
             << "    left: " << rect->left << ",\n"
             << "    top: " << rect->top << ",\n"
             << "    width: " << (rect->right - rect->left) << ",\n"
             << "    height: " << (rect->bottom - rect->top) << ",\n"
             << "}\n";
    }
    const optional <string> &pathToSave = getFilePath(info);
    if (pathToSave.has_value()) {
        cout << "  pathToSave: '" << pathToSave.value() << "'\n";
    }

    std::vector <BYTE> data;
    getScreen(data, rect);

    if (pathToSave.has_value()) {
        saveToFile(data, pathToSave.value());
    }

    // https://stackoverflow.com/questions/67602113/how-to-return-float32-array-from-c-in-node-addon-api
    Napi::Env env = info.Env();
    Napi::Uint8Array result = Napi::Uint8Array::New(env, data.size());
    for (std::size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i];
    }
    return result;
}


Napi::Object Init(Napi::Env env, Napi::Object exports) {
    signal(SIGINT, signal_callback_handler);

    exports.Set(Napi::String::New(env, "screenshot"), Napi::Function::New(env, MakeScreenshot));

    return exports;
}

NODE_API_MODULE(screener, Init
);
