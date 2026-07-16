
#include <cstdint>
#include <string>
#include <mutex>
#include <stdexcept>
#include <functional>
#include <vector>
#include <sstream>
#include <algorithm>
#include <android/log.h>

#define LOG_TAG "A64FieldHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Declarações externas (ajuste se já tiver)
extern uintptr_t getAbsoluteAddress(const char* libName, uintptr_t offset);
extern uintptr_t string2Offset(const char* s);

// Engine de hook — podem variar
extern "C" void A64HookFunction(void* target, void* replace, void** result); // algumas libs são void
extern "C" bool A64HookFunction_bool(void* target, void* replace, void** result); // outras são bool
extern "C" void MSHookFunction(void* target, void* replace, void** result);  // ARM32

#if defined(__aarch64__)
#define IS_ARM64 true
#else
#define IS_ARM64 false
#endif

// Detecta automaticamente o tipo de retorno de A64HookFunction
template<typename Ret, typename = void>
struct has_bool_return : std::false_type {};

template<typename Ret>
struct has_bool_return<Ret, std::enable_if_t<std::is_same_v<Ret, bool>>> : std::true_type {};

template<typename FnPtr>
class A64FieldHook {
    static_assert(std::is_pointer<FnPtr>::value, "FnPtr deve ser ponteiro de função");

public:
    using func_ptr_t = FnPtr;

    A64FieldHook(const char* libName, const char* offsetStr)
        : m_lib(libName), m_spec(offsetStr ? offsetStr : ""), m_resolved(false),
          m_hooked(false), m_orig(nullptr), m_addr(0) {}

    // Chamada direta como field
    template<typename... Args>
    auto operator()(Args&&... args)
        -> decltype(std::declval<func_ptr_t>()(std::forward<Args>(args)...))
    {
        auto f = get();
        if (!f) throw std::runtime_error("A64FieldHook: função não resolvida");
        return f(std::forward<Args>(args)...);
    }

    // Conversão implícita para ponteiro original
    operator func_ptr_t() { return get(); }

    // Retorna ponteiro original
    func_ptr_t get() {
        std::call_once(m_once, [this]() {
            m_addr = resolveSpec(m_spec.c_str());
            if (!m_addr) {
                LOGE("Falha ao resolver offset '%s' em '%s'", m_spec.c_str(), m_lib.c_str());
                m_orig = nullptr;
                return;
            }
            m_orig = reinterpret_cast<func_ptr_t>(m_addr);
            m_resolved = true;
            LOGI("A64FieldHook: resolvido '%s' -> 0x%lx", m_spec.c_str(), (unsigned long)m_addr);
        });
        return m_orig;
    }

    // Instala hook (funciona com qualquer A64HookFunction)
    bool Hook(void* replacement) {
        if (m_hooked) return true;

        uintptr_t addr = resolveSpec(m_spec.c_str());
        if (!addr) {
            LOGE("Hook falhou: não conseguiu resolver '%s'", m_spec.c_str());
            return false;
        }

        void* target = reinterpret_cast<void*>(addr);
        void* trampoline = nullptr;
        bool ok = false;

#if defined(__aarch64__)
        // --- ARM64 ---
        // Testa primeiro se temos a versão bool
        if constexpr (has_bool_return<decltype(A64HookFunction_bool(nullptr, nullptr, nullptr))>::value) {
            ok = A64HookFunction_bool(target, replacement, &trampoline);
        } else {
            A64HookFunction(target, replacement, &trampoline);
            ok = true; // assume sucesso
        }
#else
        // --- ARM32 ---
        MSHookFunction(target, replacement, &trampoline);
        ok = true;
#endif

        if (ok) {
            m_orig = reinterpret_cast<func_ptr_t>(trampoline ? trampoline : target);
            m_hooked = true;
            LOGI("Hook instalado: '%s' (0x%lx) [%s]",
                 m_spec.c_str(), (unsigned long)addr, IS_ARM64 ? "ARM64" : "ARM32");
        } else {
            LOGE("A64HookFunction falhou: '%s' (0x%lx)", m_spec.c_str(), (unsigned long)addr);
        }

        return ok;
    }

private:
    // --- Parser básico para offsets e pointer chains ---
    static uintptr_t parseHex(const std::string& s) {
        try {
            std::string t = s;
            if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0)
                return std::stoull(t, nullptr, 16);
            return std::stoull(t, nullptr, 16);
        } catch (...) { return 0; }
    }

    uintptr_t resolveSpec(const char* spec) {
        if (!spec || !*spec) return 0;
        std::string expr(spec);

        // Split por "->" (derefs)
        std::vector<std::string> derefs;
        std::stringstream ss(expr);
        std::string part;
        while (std::getline(ss, part, '>')) {
            if (!part.empty() && part != "-") derefs.push_back(part);
        }

        // Primeiro token é offset base
        uintptr_t baseOff = parseHex(derefs[0]);
        uintptr_t addr = getAbsoluteAddress(m_lib.c_str(), baseOff);
        if (!addr) addr = baseOff;

        // Processa os demais
        for (size_t i = 1; i < derefs.size(); i++) {
            std::string s = derefs[i];
            s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
            uintptr_t field = parseHex(s);
            if (!addr) return 0;
            uintptr_t ptrAddr = addr + field;
            if (sizeof(uintptr_t) == 8)
                addr = *reinterpret_cast<uint64_t*>(ptrAddr);
            else
                addr = *reinterpret_cast<uint32_t*>(ptrAddr);
        }

        return addr;
    }

private:
    std::string m_lib;
    std::string m_spec;
    bool m_resolved;
    bool m_hooked;
    func_ptr_t m_orig;
    uintptr_t m_addr;
    std::once_flag m_once;
};
