#include <Windows.h>
#include <Windowsx.h>
#include <Winuser.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <optional>

using namespace Gdiplus;
#pragma comment (lib, "Gdiplus.lib")


using namespace std;

int GetEncoderClsid(const WCHAR *format, CLSID *pClsid) {
    UINT num = 0;          // number of image encoders
    UINT size = 0;         // size of the image encoder array in bytes

    ImageCodecInfo *pImageCodecInfo = NULL;

    GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;  // Failure

    pImageCodecInfo = (ImageCodecInfo *) (malloc(size));
    if (pImageCodecInfo == NULL)
        return -1;  // Failure

    GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

// https://github.com/Kuzat/screenshot-node/blob/master/src/windows/prtscn_windows.cpp
// https://stackoverflow.com/questions/51383896/c-gdibitmap-to-png-image-in-memory
// https://stackoverflow.com/questions/366768/convert-bitmap-to-png-in-memory-in-c-win32
void getScreen(/*const int x, const int y, int W, int H, */std::vector<BYTE> &data, std::optional<RECT> rectOpt) {
    SetProcessDPIAware(); //todo check in other places
    int x, y, W, H;
    if (rectOpt.has_value()) {
        x = rectOpt->left;
        y = rectOpt->top;
        W = rectOpt->right - rectOpt->left;
        H = rectOpt->bottom - rectOpt->top;
    }
    else
    {
        //todo support screen all displays or specific display, but now it is not empty optional
        W = GetSystemMetrics(SM_CXSCREEN);
        H = GetSystemMetrics(SM_CYSCREEN);
    }
    CoInitialize(NULL);
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    {
        HBITMAP *bmpHandleP;
        {
            HDC hdc = GetDC(NULL); // get the desktop device context
            HDC hDest = CreateCompatibleDC(hdc); // create a device context to use yourself
            // create a bitmap
            auto r = CreateCompatibleBitmap(hdc, W, H);
            bmpHandleP = &(r);

            // use the previously created device context with the bitmap
            auto oldbmp = SelectObject(hDest, *bmpHandleP);

            // copy from the desktop device context to the bitmap device context
            BitBlt(hDest, 0, 0, W, H, hdc, x, y, SRCCOPY);
            SelectObject(hDest, oldbmp);
            ReleaseDC(NULL, hdc);
            DeleteDC(hDest);
        }

        HBITMAP bmpHandle = *bmpHandleP;
        {
            Gdiplus::Bitmap bitmap(bmpHandle, nullptr);

            IStream *istream = nullptr;
            // this will be released in istream->Release();
            if (CreateStreamOnHGlobal(NULL, TRUE, &istream) != S_OK) {
                std::cout << "hr !=  S_OK\n";
                return;
            }
            CLSID clsid;
            GetEncoderClsid(L"image/png", &clsid);

            Gdiplus::Status status = bitmap.Save(istream, &clsid);
            if (status != Gdiplus::Status::Ok) {
                std::cout << "status != Gdiplus::Status::Ok\n";
                return;
            }
            //get memory handle associated with istream
            HGLOBAL hg = NULL;
            GetHGlobalFromStream(istream, &hg);

            //copy IStream to buffer
            SIZE_T bufsize = GlobalSize(hg);
            data.resize(bufsize);

            //lock & unlock memory
            LPVOID pimage = GlobalLock(hg);
            memcpy(&data[0], pimage, bufsize);
            GlobalUnlock(hg);

            // release will automatically free the memory allocated in CreateStreamOnHGlobal
            istream->Release();
        }
        DeleteObject(bmpHandle);
    }
    GdiplusShutdown(gdiplusToken);
    CoUninitialize();
}
