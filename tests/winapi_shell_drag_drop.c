#include <windows.h>
#include <shellapi.h>

static volatile int call_shellapi_surface;

static void probe_shellapi_calls(void)
{
    char path_a[MAX_PATH] = {0};
    WCHAR path_w[MAX_PATH] = {0};
    POINT point = {0};
    int argc = 0;
    HDROP drop = (HDROP)0;

    (void)ShellExecuteA((HWND)0, (LPCSTR)0, "", (LPCSTR)0, (LPCSTR)0, SW_HIDE);
    (void)ShellExecuteW((HWND)0, (LPCWSTR)0, L"", (LPCWSTR)0, (LPCWSTR)0, SW_HIDE);
    (void)FindExecutableA("", (LPCSTR)0, path_a);
    (void)FindExecutableW(L"", (LPCWSTR)0, path_w);
    (void)CommandLineToArgvW(L"", &argc);

    (void)DragQueryFileA(drop, 0, path_a, MAX_PATH);
    (void)DragQueryFileW(drop, 0, path_w, MAX_PATH);
    (void)DragQueryPoint(drop, &point);
    DragFinish(drop);
    DragAcceptFiles((HWND)0, FALSE);

#ifdef UNICODE
    (void)ShellExecute((HWND)0, (LPCWSTR)0, L"", (LPCWSTR)0, (LPCWSTR)0, SW_HIDE);
    (void)FindExecutable(L"", (LPCWSTR)0, path_w);
    (void)DragQueryFile(drop, 0, path_w, MAX_PATH);
#else
    (void)ShellExecute((HWND)0, (LPCSTR)0, "", (LPCSTR)0, (LPCSTR)0, SW_HIDE);
    (void)FindExecutable("", (LPCSTR)0, path_a);
    (void)DragQueryFile(drop, 0, path_a, MAX_PATH);
#endif
}

int main(void)
{
    if (call_shellapi_surface) {
        probe_shellapi_calls();
    }
#ifdef UNICODE
    if (DragQueryFile != DragQueryFileW || ShellExecute != ShellExecuteW ||
        FindExecutable != FindExecutableW) {
        return 2;
    }
#else
    if (DragQueryFile != DragQueryFileA || ShellExecute != ShellExecuteA ||
        FindExecutable != FindExecutableA) {
        return 2;
    }
#endif
    return 0;
}
