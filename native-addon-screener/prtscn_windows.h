#ifndef PRTSCN_WINDOWS_H_
#define PRTSCN_WINDOWS_H_

#include <vector>
#include <Windows.h>

void getScreen(std::vector<BYTE> &data, std::optional<RECT> rect);

#endif
