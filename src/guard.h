#pragma once
extern "C" int __cdecl RunGuarded(void (__cdecl *body)(void*), void* context);
template<class F> bool Guard(F body) {
    return RunGuarded([](void* context) { (*static_cast<F*>(context))(); }, &body)!=0;
}
