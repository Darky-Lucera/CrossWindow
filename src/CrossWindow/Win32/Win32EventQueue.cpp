#include "Win32EventQueue.h"
#include "../Common/Window.h"

#include "Shobjidl.h"
#include "dwmapi.h"
#include <windowsx.h>

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((USHORT)0x01)
#endif
#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif

RAWINPUTDEVICE Rid[1];

namespace xwin
{
Win32EventQueue::Win32EventQueue() { initialized = false; }

void Win32EventQueue::update()
{
    MSG msg = {};

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        // Translate virtual key messages
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT Win32EventQueue::pushEvent(MSG msg, Window* window)
{
    UINT message = msg.message;
    LRESULT result = 0;
    // TODO: hwnd to xwin::Window unordered_map, when xwin::Window closes, it
    // sends a message to the event queue to remove that hwnd
    // and any remaining events that match that xwin::Window

    if (!initialized)
    {
        initialized = true;
        Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
        Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
        Rid[0].dwFlags = RIDEV_INPUTSINK;
        Rid[0].hwndTarget = window->getDelegate().hwnd;
        RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));
    }

	xwin::Event e = xwin::Event(xwin::EventType::None, window);

    switch (message)
    {
    case WM_CREATE:
    {
        e = xwin::Event(xwin::EventType::Create, window);
		//repaint window when borderless to avoid 6px resizable border.
        MoveWindow(window->getDelegate().hwnd, 0, 0, window->getDesc().width,
                   window->getDesc().height+8, true);
        break;
    }
    case WM_PAINT:
    {
        /*
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(window->getDelegate().hwnd, &ps);

        RECT rc = ps.rcPaint;
        BP_PAINTPARAMS params = {sizeof(params), BPPF_NOCLIP | BPPF_ERASE};
        HDC memdc;
        HPAINTBUFFER hbuffer =
            BeginBufferedPaint(hdc, &rc, BPBF_TOPDOWNDIB, &params, &memdc);

   HBRUSH brush = CreateSolidBrush(RGB(23, 26, 30));
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);

        BufferedPaintSetAlpha(hbuffer, &rc, 255);
        EndBufferedPaint(hbuffer, TRUE);

        EndPaint(window->getDelegate().hwnd, &ps);
		*/
        e = xwin::Event(xwin::EventType::Paint, window);
        break;
    }
    case WM_ERASEBKGND:
    {
        break;
    }
    case WM_DESTROY:
    {
        e = xwin::Event(xwin::EventType::Close, window);
        break;
    }
    case WM_SETFOCUS:
    {
        e = xwin::Event(xwin::FocusData(true), window);
        break;
    }
    case WM_KILLFOCUS:
    {
        e = xwin::Event(xwin::FocusData(false), window);
        break;
    }
    case WM_NCHITTEST:
	{
        POINT cursor = POINT{GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam)};
        // identify borders and corners to allow resizing the window.
        // Note: On Windows 10, windows behave differently and
        // allow resizing outside the visible window frame.
        // This implementation does not replicate that behavior.
        bool borderless_resize = true;
        bool borderless_drag = true;
        const POINT border{::GetSystemMetrics(SM_CXFRAME) +
                               ::GetSystemMetrics(SM_CXPADDEDBORDER),
                           ::GetSystemMetrics(SM_CYFRAME) +
                               ::GetSystemMetrics(SM_CXPADDEDBORDER)};
        RECT windowRect;
        if (!::GetWindowRect(window->getDelegate().hwnd, &windowRect))
        {
            result =  HTNOWHERE;
        }

        const auto drag = borderless_drag ? HTCAPTION : HTCLIENT;

        enum region_mask
        {
            client = 0b0000,
            left = 0b0001,
            right = 0b0010,
            top = 0b0100,
            bottom = 0b1000,
        };

        result =
            left * (cursor.x < (windowRect.left + border.x)) |
            right * (cursor.x >= (windowRect.right - border.x)) |
            top * (cursor.y < (windowRect.top + border.y)) |
            bottom * (cursor.y >= (windowRect.bottom - border.y));

        switch (result)
        {
        case left:
            result =  borderless_resize ? HTLEFT : drag;
        case right:
            result =  borderless_resize ? HTRIGHT : drag;
        case top:
            result =  borderless_resize ? HTTOP : drag;
        case bottom:
            result =  borderless_resize ? HTBOTTOM : drag;
        case top | left:
            result =  borderless_resize ? HTTOPLEFT : drag;
        case top | right:
            result =  borderless_resize ? HTTOPRIGHT : drag;
        case bottom | left:
            result =  borderless_resize ? HTBOTTOMLEFT : drag;
        case bottom | right:
            result =  borderless_resize ? HTBOTTOMRIGHT : drag;
        case client:
            result =  drag;
        default:
            result =  HTNOWHERE;
        }
        break;
	}
    case WM_MOUSEWHEEL:
    {
        short modifiers = LOWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseWheelData(
                GET_WHEEL_DELTA_WPARAM(msg.wParam) / WHEEL_DELTA,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_LBUTTONDOWN:
    {
        short modifiers = LOWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                MouseInput::Left, ButtonState::Pressed,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_MBUTTONDOWN:
    {
        short modifiers = LOWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                MouseInput::Middle, ButtonState::Pressed,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_RBUTTONDOWN:
    {
        short modifiers = LOWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                MouseInput::Right, ButtonState::Pressed,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_XBUTTONDOWN:
    {
        short modifiers = LOWORD(msg.wParam);
        short x = HIWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                x & XBUTTON1 ? MouseInput::Button4 : MouseInput::Button5,
                ButtonState::Pressed,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_XBUTTONUP:
    {
        short modifiers = LOWORD(msg.wParam);
        short x = HIWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                x & XBUTTON1 ? MouseInput::Button4 : MouseInput::Button5,
                ButtonState::Released,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_LBUTTONDBLCLK:
        // Perhaps there should be an event for this in the future
        break;
    case WM_LBUTTONUP:
    {
        short modifiers = LOWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                MouseInput::Left, ButtonState::Released,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_MBUTTONUP:
    {
        short modifiers = LOWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                MouseInput::Middle, ButtonState::Released,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_RBUTTONUP:
    {
        short modifiers = LOWORD(msg.wParam);
        e = xwin::Event(
            xwin::MouseInputData(
                MouseInput::Right, ButtonState::Released,
                xwin::ModifierState(modifiers & MK_CONTROL, modifiers & MK_ALT,
                                    modifiers & MK_SHIFT, modifiers & 0)),
            window);
        break;
    }
    case WM_INPUT:
    {
        UINT dwSize;

        GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, NULL, &dwSize,
                        sizeof(RAWINPUTHEADER));
        LPBYTE lpb = new BYTE[dwSize];
        if (lpb == NULL)
        {
            // return 0;
        }

        if (GetRawInputData((HRAWINPUT)msg.lParam, RID_INPUT, lpb, &dwSize,
                            sizeof(RAWINPUTHEADER)) != dwSize)
            OutputDebugString(
                TEXT("GetRawInputData does not return correct size !\n"));

        RAWINPUT* raw = (RAWINPUT*)lpb;

        if (raw->header.dwType == RIM_TYPEKEYBOARD)
        {

            raw->data.keyboard.MakeCode, raw->data.keyboard.Flags,
                raw->data.keyboard.Reserved,
                raw->data.keyboard.ExtraInformation, raw->data.keyboard.Message,
                raw->data.keyboard.VKey;
        }
        else if (raw->header.dwType == RIM_TYPEMOUSE)
        {

            raw->data.mouse.usFlags, raw->data.mouse.ulButtons,
                raw->data.mouse.usButtonFlags, raw->data.mouse.usButtonData,
                raw->data.mouse.ulRawButtons, raw->data.mouse.lLastX,
                raw->data.mouse.lLastY, raw->data.mouse.ulExtraInformation;

            e = xwin::Event(
                xwin::MouseRawData(static_cast<int>(raw->data.mouse.lLastX),
                                   static_cast<int>(raw->data.mouse.lLastY)),
                window);

            // printf("%.3f, %.3f\n",
            // raw->data.mouse.lLastX,raw->data.mouse.lLastY)
        }

        delete[] lpb;
        break;
    }
    case WM_MOUSEMOVE:
    {
        HWND hwnd = window->getDelegate().hwnd;
        // Extract the mouse local coordinates
        int x = static_cast<short>(LOWORD(msg.lParam));
        int y = static_cast<short>(HIWORD(msg.lParam));

        // Get the client area of the window
        RECT area;
        GetClientRect(hwnd, &area);
        /*
        // Capture the mouse in case the user wants to drag it outside
        if ((msg.wParam & (MK_LBUTTON | MK_MBUTTON | MK_RBUTTON | MK_XBUTTON1 |
        MK_XBUTTON2)) == 0)
        {
            // Only release the capture if we really have it
            if (GetCapture() == hwnd)
                ReleaseCapture();
        }
        else if (GetCapture() != hwnd)
        {
            // Set the capture to continue receiving mouse events
            SetCapture(hwnd);
        }

        // If the cursor is outside the client area...
        if ((x < area.left) || (x > area.right) || (y < area.top) || (y >
        area.bottom))
        {
            // and it used to be inside, the mouse left it.
            if (m_mouseInside)
            {
                m_mouseInside = false;

                // No longer care for the mouse leaving the window
                setTracking(false);

                // Generate a MouseLeft event
                Event event;
                event.type = Event::MouseLeft;
                pushEvent(event);
            }
        }
        else
        {
            // and vice-versa
            if (!m_mouseInside)
            {
                m_mouseInside = true;

                // Look for the mouse leaving the window
                setTracking(true);

                // Generate a MouseEntered event
                Event event;
                event.type = Event::MouseEntered;
                pushEvent(event);
            }
        }*/

        e = xwin::Event(xwin::MouseMoveData(
                           static_cast<unsigned>(
                               area.left <= x && x <= area.right ? x - area.left
                                                                 : 0xFFFFFFFF),
                           static_cast<unsigned>(
                               area.top <= y && y <= area.bottom ? y - area.top
                                                                 : 0xFFFFFFFF),
                           static_cast<unsigned>(x), static_cast<unsigned>(y),
                           static_cast<int>(x - prevMouseX),
                           static_cast<int>(y - prevMouseY)),
                       window);
        prevMouseX = static_cast<unsigned>(x);
        prevMouseY = static_cast<unsigned>(y);
        break;
    }
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_CHAR:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        Key d;

        switch (msg.wParam)
        {
        case VK_ESCAPE:
            d = Key::Escape;
            break;
        case 0x30:
            d = Key::Num0;
            break;
        case 0x31:
            d = Key::Num1;
            break;
        case 0x32:
            d = Key::Num2;
            break;
        case 0x33:
            d = Key::Num3;
            break;
        case 0x34:
            d = Key::Num4;
            break;
        case 0x35:
            d = Key::Num5;
            break;
        case 0x36:
            d = Key::Num6;
            break;
        case 0x37:
            d = Key::Num7;
            break;
        case 0x38:
            d = Key::Num8;
            break;
        case 0x39:
            d = Key::Num9;
            break;
        case 0x41:
            d = Key::A;
            break;
        case 0x42:
            d = Key::B;
            break;
        case 0x43:
            d = Key::C;
            break;
        case 0x44:
            d = Key::D;
            break;
        case 0x45:
            d = Key::E;
            break;
        case 0x46:
            d = Key::F;
            break;
        case 0x47:
            d = Key::G;
            break;
        case 0x48:
            d = Key::H;
            break;
        case 0x49:
            d = Key::I;
            break;
        case 0x4A:
            d = Key::J;
            break;
        case 0x4B:
            d = Key::K;
            break;
        case 0x4C:
            d = Key::L;
            break;
        case 0x4D:
            d = Key::M;
            break;
        case 0x4E:
            d = Key::N;
            break;
        case 0x4F:
            d = Key::O;
            break;
        case 0x50:
            d = Key::P;
            break;
        case 0x51:
            d = Key::Q;
            break;
        case 0x52:
            d = Key::R;
            break;
        case 0x53:
            d = Key::S;
            break;
        case 0x54:
            d = Key::T;
            break;
        case 0x55:
            d = Key::U;
            break;
        case 0x56:
            d = Key::V;
            break;
        case 0x57:
            d = Key::W;
            break;
        case 0x58:
            d = Key::X;
            break;
        case 0x59:
            d = Key::Y;
            break;
        case 0x5A:
            d = Key::Z;
            break;
        case VK_NUMPAD0:
            d = Key::Numpad0;
            break;
        case VK_NUMPAD1:
            d = Key::Numpad1;
            break;
        case VK_NUMPAD2:
            d = Key::Numpad2;
            break;
        case VK_NUMPAD3:
            d = Key::Numpad3;
            break;
        case VK_NUMPAD4:
            d = Key::Numpad4;
            break;
        case VK_NUMPAD5:
            d = Key::Numpad5;
            break;
        case VK_NUMPAD6:
            d = Key::Numpad6;
            break;
        case VK_NUMPAD7:
            d = Key::Numpad7;
            break;
        case VK_NUMPAD8:
            d = Key::Numpad8;
            break;
        case VK_NUMPAD9:
            d = Key::Numpad9;
            break;
        case VK_UP:
            d = Key::Up;
            break;
        case VK_LEFT:
            d = Key::Left;
            break;
        case VK_DOWN:
            d = Key::Down;
            break;
        case VK_RIGHT:
            d = Key::Right;
            break;
        case VK_SPACE:
            d = Key::Space;
            break;
        case VK_HOME:
            d = Key::Home;
            break;
        case VK_F1:
            d = Key::F1;
            break;
        case VK_F2:
            d = Key::F2;
            break;
        case VK_F3:
            d = Key::F3;
            break;
        case VK_F4:
            d = Key::F4;
            break;
        case VK_F5:
            d = Key::F5;
            break;
        case VK_F6:
            d = Key::F6;
            break;
        case VK_F7:
            d = Key::F7;
            break;
        case VK_F8:
            d = Key::F8;
            break;
        case VK_F9:
            d = Key::F9;
            break;
        case VK_F10:
            d = Key::F10;
            break;
        case VK_F11:
            d = Key::F11;
            break;
        case VK_F12:
            d = Key::F12;
            break;

        default:
            d = Key::KeysMax;
            break;
        }

        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
        {
            e = xwin::Event(
                KeyboardData(d, ButtonState::Pressed, ModifierState()), window);
        }
        else if (message == WM_KEYUP || message == WM_SYSKEYUP)
        {
            e = xwin::Event(
                KeyboardData(d, ButtonState::Released, ModifierState()),
                window);
        }
        break;
    }
    case WM_SIZE:
    {
        unsigned width, height;
        width = static_cast<unsigned>((UINT64)msg.lParam & 0xFFFF);
        height = static_cast<unsigned>((UINT64)msg.lParam >> 16);

        e = xwin::Event(ResizeData(width, height, false), window);
        break;
    }
    case WM_SIZING:
    {
        unsigned WIDTH = 320;
        unsigned HEIGHT = 320;
        unsigned width, height;
			unsigned STEP = 1;
        PRECT rectp = (PRECT)msg.lParam;
        HWND hwnd = window->getDelegate().hwnd;
        // Get the window and client dimensions
        tagRECT wind, rect;
        GetWindowRect(hwnd, &wind);
        GetClientRect(hwnd, &rect);

        // Edges
        //int border = (wind.right - wind.left) - rect.right;
        //int header =
        //    (wind.bottom - wind.top) - rect.bottom;

        // Window minimum width and height
        //int width = WIDTH + border;
        //int height = HEIGHT + rect.bottom + header;

        // Minimum size
        //if (rectp->right - rectp->left < width)
        //    rectp->right = rectp->left + width;

        //if (rectp->bottom - rectp->top < height)
        //    rectp->bottom = rectp->top + height;

        // Maximum width
        //if (rectp->right - rectp->left > STEP + border)
        //    rectp->right = rectp->left + STEP + border;

        // Offered width and height
        width = rectp->right - rectp->left;
        //-15;
        height = rectp->bottom - rectp->top;
        //-39;
        /*
        switch (msg.wParam)
        {
        case WMSZ_LEFT:
        case WMSZ_RIGHT:
            height = (((width - border) * HEIGHT) / WIDTH) +
                     rect.bottom + header;
            rectp->bottom = rectp->top + height;
            break;

        case WMSZ_TOP:
        case WMSZ_BOTTOM:
            width =
                ((((height - rect.bottom) - header) * WIDTH) / HEIGHT) +
                border;
            rectp->right = rectp->left + width;
            break;

        default:
            width =
                ((((height - rect.bottom) - header) * WIDTH) / HEIGHT) +
                border;
            rectp->right = rectp->left + width;
            break;
        }*/
        
        e = xwin::Event(ResizeData(width, height, true), window);
        break;
    }
    case WM_NCCALCSIZE:
	{
        if (msg.lParam)
        {
            NCCALCSIZE_PARAMS* sz = (NCCALCSIZE_PARAMS*)msg.lParam;
            sz->rgrc[0].top -= 6;
        }
        break;
	}
    default:
        // Do nothing
        break;
    }
	if (e.type != EventType::None)
	{
        mQueue.emplace(e);
	}
    window->getDelegate().executeEventCallback(e);
    return result;
}

const Event& Win32EventQueue::front() { return mQueue.front(); }

void Win32EventQueue::pop() { mQueue.pop(); }

bool Win32EventQueue::empty() { return mQueue.empty(); }
size_t Win32EventQueue::size() { return mQueue.size(); }
}