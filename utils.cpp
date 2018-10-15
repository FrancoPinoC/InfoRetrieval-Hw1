#include "utils.h"
#include <iostream>
// With thanks to https://stackoverflow.com/a/4430900/4192226
std::string extractNameFromPath(const std::string &fullPath) {
    // To be file name with the extension (if it has one)
    std::string filename;
    // Only the name without the extension
    std::string name;
    size_t sep = fullPath.find_last_of("\\/");
    if (sep != std::string::npos)
        filename = fullPath.substr(sep + 1, fullPath.size() - sep - 1);

    size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos)
    {
        name = filename.substr(0, dot);
    }
    else
    {
        name = filename;
    }
    return name;
}