#pragma once

// «Abrir los magnet y los .torrent con HNTorrent»: solo cuando la persona pulsa el botón en Preferencias
// (excepción aprobada por Sergio el 06/11/2026). Todo va en HKEY_CURRENT_USER y nada pide ser administrador.
//
// No se toca la clave «magnet» ni «.torrent» de otros programas: HNTorrent se anota como un programa más que sabe
// abrirlos (sus propios ProgID, OpenWithProgids y Capabilities) y Windows deja que la persona lo elija en
// Configuración > Aplicaciones predeterminadas. «Quitar» borra solo lo que se anotó aquí.

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#include <string>

namespace pt::UI::Asociaciones
{
    inline const wchar_t* ProgIdTorrent = L"Software\\Classes\\HNTorrent.torrent";
    inline const wchar_t* ProgIdMagnet = L"Software\\Classes\\HNTorrent.magnet";
    inline const wchar_t* AbrirConTorrent = L"Software\\Classes\\.torrent\\OpenWithProgids";
    inline const wchar_t* Capacidades = L"Software\\HNTorrent\\Capabilities";
    inline const wchar_t* Registradas = L"Software\\RegisteredApplications";

    inline std::wstring EsteExe()
    {
        std::wstring ruta(MAX_PATH, L'\0');
        for (;;)
        {
            DWORD n = GetModuleFileNameW(nullptr, ruta.data(), (DWORD)ruta.size());
            if (n == 0) { return {}; }
            if (n < ruta.size()) { ruta.resize(n); return ruta; }
            if (ruta.size() >= 32768) { return {}; }
            ruta.resize(ruta.size() * 2);
        }
    }

    inline bool Poner(const wchar_t* clave, const wchar_t* nombre, const std::wstring& valor)
    {
        return RegSetKeyValueW(HKEY_CURRENT_USER, clave, nombre, REG_SZ, valor.c_str(),
                               (DWORD)((valor.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    }

    inline bool AnotarProgId(const wchar_t* progId, const std::wstring& descripcion, const std::wstring& exe, bool protocolo)
    {
        std::wstring base(progId);
        bool ok = Poner(progId, nullptr, descripcion);
        if (protocolo) { ok = Poner(progId, L"URL Protocol", L"") && ok; }
        ok = Poner((base + L"\\DefaultIcon").c_str(), nullptr, L"\"" + exe + L"\",0") && ok;
        ok = Poner((base + L"\\shell\\open\\command").c_str(), nullptr, L"\"" + exe + L"\" \"%1\"") && ok;
        return ok;
    }

    // ¿Ya está anotado, y con este mismo HNTorrent.exe? (si se movió la carpeta, hay que volver a pulsar)
    inline bool EstaAnotado()
    {
        wchar_t valor[32768 / 2] = {};
        DWORD largo = sizeof(valor);
        std::wstring clave = std::wstring(ProgIdTorrent) + L"\\shell\\open\\command";
        if (RegGetValueW(HKEY_CURRENT_USER, clave.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, valor, &largo) != ERROR_SUCCESS)
        {
            return false;
        }
        return std::wstring(valor) == L"\"" + EsteExe() + L"\" \"%1\"";
    }

    inline bool Anotar(const std::wstring& descripcionTorrent, const std::wstring& descripcionMagnet,
                       const std::wstring& descripcionPrograma)
    {
        std::wstring exe = EsteExe();
        if (exe.empty() || exe.find(L'"') != std::wstring::npos) { return false; }

        bool ok = AnotarProgId(ProgIdTorrent, descripcionTorrent, exe, false);
        ok = AnotarProgId(ProgIdMagnet, descripcionMagnet, exe, true) && ok;
        ok = RegSetKeyValueW(HKEY_CURRENT_USER, AbrirConTorrent, L"HNTorrent.torrent", REG_NONE, nullptr, 0) == ERROR_SUCCESS && ok;

        std::wstring cap(Capacidades);
        ok = Poner(Capacidades, L"ApplicationName", L"HNTorrent") && ok;
        ok = Poner(Capacidades, L"ApplicationDescription", descripcionPrograma) && ok;
        ok = Poner((cap + L"\\FileAssociations").c_str(), L".torrent", L"HNTorrent.torrent") && ok;
        ok = Poner((cap + L"\\URLAssociations").c_str(), L"magnet", L"HNTorrent.magnet") && ok;
        ok = Poner(Registradas, L"HNTorrent", Capacidades) && ok;

        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        return ok;
    }

    // Borra solo lo que anotó HNTorrent. Si Windows ya lo tenía elegido para abrirlos, deja de ofrecerlo.
    inline void Quitar()
    {
        RegDeleteTreeW(HKEY_CURRENT_USER, ProgIdTorrent);
        RegDeleteTreeW(HKEY_CURRENT_USER, ProgIdMagnet);
        RegDeleteKeyValueW(HKEY_CURRENT_USER, AbrirConTorrent, L"HNTorrent.torrent");
        RegDeleteTreeW(HKEY_CURRENT_USER, Capacidades);
        RegDeleteKeyValueW(HKEY_CURRENT_USER, Registradas, L"HNTorrent");
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }

    // Abre Configuración en la página de HNTorrent, donde la persona elige qué abre con él
    inline void AbrirConfiguracion()
    {
        ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps?registeredAppUser=HNTorrent", nullptr, nullptr, SW_SHOWNORMAL);
    }
}
