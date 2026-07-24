#include <stdio.h>
#include <tchar.h>

typedef errno_t(__cdecl *narrow_freopen_s_fn)(FILE **, const char *, const char *, FILE *);
typedef errno_t(__cdecl *generic_freopen_s_fn)(FILE **, const TCHAR *, const TCHAR *, FILE *);

static narrow_freopen_s_fn narrow_freopen_s = freopen_s;
static generic_freopen_s_fn generic_freopen_s = _tfreopen_s;

int main(void) {
	return narrow_freopen_s != NULL && generic_freopen_s != NULL ? 0 : 1;
}
