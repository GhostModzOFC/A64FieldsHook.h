#include "A64FieldHook.h"

// ═══════════════════════════════════════════════════════════════
// EXEMPLO 1: Hook simples - Vida infinita
// ═══════════════════════════════════════════════════════════════

// Declara o hook com o tipo da funcao original
// Nesse caso: int GetHealth(void* instance)
A64FieldHook<int(*)(void*)> getHealth("libil2cpp.so", "0x1A2B3C");

// Sua funcao fake que substitui a original
int getHealth_hook(void* instance) {
    // Chama a original pra nao crashar
    int vidaReal = getHealth(instance);

    // Retorna sempre 9999 (vida infinita)
    return 9999;
}

// ═══════════════════════════════════════════════════════════════
// EXEMPLO 2: Hook de dano - Dano multiplicado
// ═══════════════════════════════════════════════════════════════

// void DealDamage(void* instance, void* target, float damage)
A64FieldHook<void(*)(void*, void*, float)> dealDamage("libil2cpp.so", "0x2C4D5E");

void dealDamage_hook(void* instance, void* target, float damage) {
    // Multiplica o dano por 100x
    dealDamage(instance, target, damage * 100.0f);
}

// ═══════════════════════════════════════════════════════════════
// EXEMPLO 3: Hook bool - Sempre acertar (no recoil/no spread)
// ═══════════════════════════════════════════════════════════════

// bool IsRecoilEnabled(void* weapon)
A64FieldHook<bool(*)(void*)> isRecoilEnabled("libil2cpp.so", "0x3F4A5B");

bool isRecoilEnabled_hook(void* weapon) {
    // Sempre retorna false = sem recuo
    return false;
}

// ═══════════════════════════════════════════════════════════════
// EXEMPLO 4: Hook com pointer chain
// ═══════════════════════════════════════════════════════════════

// Navega: base + 0x100 -> deref -> + 0x20 -> deref -> funcao
A64FieldHook<float(*)(void*)> getSpeed("libil2cpp.so", "0x100->0x20->0x8");

float getSpeed_hook(void* instance) {
    // Speed hack: velocidade 5x
    float speedReal = getSpeed(instance);
    return speedReal * 5.0f;
}

// ═══════════════════════════════════════════════════════════════
// EXEMPLO 5: Hook void - Desabilitar funcao completamente
// ═══════════════════════════════════════════════════════════════

// void ApplyBan(void* instance, int reason)
A64FieldHook<void(*)(void*, int)> applyBan("libil2cpp.so", "0x5A6B7C");

void applyBan_hook(void* instance, int reason) {
    // Nao faz nada = anti-ban (a funcao original nunca executa)
    return;
}

// ═══════════════════════════════════════════════════════════════
// ONDE INSTALAR OS HOOKS (no seu hack_thread ou onload)
// ═══════════════════════════════════════════════════════════════

void *hack_thread(void *) {
    // Espera a lib carregar
    while (!isLibraryLoaded("libil2cpp.so")) {
        usleep(100000);
    }
    usleep(2000000); // espera 2s extra pra estabilizar

    // Instala todos os hooks
    getHealth.Hook((void*)getHealth_hook);
    dealDamage.Hook((void*)dealDamage_hook);
    isRecoilEnabled.Hook((void*)isRecoilEnabled_hook);
    getSpeed.Hook((void*)getSpeed_hook);
    applyBan.Hook((void*)applyBan_hook);

    return NULL;
}

// ═══════════════════════════════════════════════════════════════
// EXEMPLO COM TOGGLE (ligar/desligar pelo menu)
// ═══════════════════════════════════════════════════════════════

bool bGodMode = false;
bool bDamageHack = false;
bool bSpeedHack = false;

A64FieldHook<int(*)(void*, float)> takeDamage("libil2cpp.so", "0x7E8F9A");

int takeDamage_hook(void* instance, float amount) {
    if (bGodMode) {
        // God mode ligado: ignora dano
        return 0;
    }
    // God mode desligado: funciona normal
    return takeDamage(instance, amount);
}

A64FieldHook<float(*)(void*)> getMovSpeed("libil2cpp.so", "0x8A9B0C");

float getMovSpeed_hook(void* instance) {
    float speed = getMovSpeed(instance);
    if (bSpeedHack) {
        return speed * 3.0f;
    }
    return speed;
}

// No seu menu ImGui voce faz:
// ImGui::Checkbox("God Mode", &bGodMode);
// ImGui::Checkbox("Speed Hack", &bSpeedHack);