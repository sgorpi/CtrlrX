#include "LinuxDpiScale.h"

#if defined(__linux__)
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <X11/Xlib.h>
#include <X11/Xresource.h>

static double get_scale_from_env(const char* name)
{
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0')
        return 0.0;

    char* end = nullptr;
    const double scale = std::strtod(value, &end);
    if (end == value || !std::isfinite(scale))
        return 0.0;

    return scale < 1.0 ? 1.0 : scale;
}

double ctrlrx_get_linux_scale_factor()
{
    const double ctrlrxScale = get_scale_from_env("CTRLRX_SCALE_FACTOR");
    if (ctrlrxScale > 0.0)
        return ctrlrxScale;

    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
        return 1.0;

    XrmInitialize();

    double dpi = 96.0;
    char* rms = XResourceManagerString(display);
    if (rms != nullptr)
    {
        XrmDatabase db = XrmGetStringDatabase(rms);
        if (db != nullptr)
        {
            char* type = nullptr;
            XrmValue value = { 0 };

            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value)
                && type != nullptr
                && std::strcmp(type, "String") == 0
                && value.addr != nullptr)
            {
                char* end = nullptr;
                const double xftDpi = std::strtod(value.addr, &end);
                if (xftDpi > 0.0 && std::isfinite(xftDpi))
                    dpi = xftDpi;
            }

            XrmDestroyDatabase(db);
        }
    }

    XCloseDisplay(display);
    return dpi / 96.0;
}

#else

double ctrlrx_get_linux_scale_factor()
{
    return 1.0;
}

#endif
