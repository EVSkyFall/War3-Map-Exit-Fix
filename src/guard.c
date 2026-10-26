/* Small C ABI boundary: compiled with Clang's MSVC target for native x86 SEH.
 * No Windows SDK headers or MSVC libraries are required by this source.
 * The callbacks must not own C++ resources that require unwinding.
 */
int __cdecl RunGuarded(void (__cdecl *body)(void*), void* context) {
    __try { body(context); return 1; }
    __except(1) { return 0; }
}
