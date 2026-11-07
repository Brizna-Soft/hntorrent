#pragma once

// Lo común del menú Ayuda de Brizna Soft, igual que en sus otros programas (ayuda.cpp de Atarraya, Jícaro, Mapache,
// Guara y Chachalaca): las direcciones del estudio, el formulario de fallas con la versión ya puesta y los datos para
// el reporte. Nada personal: solo la versión de HNTorrent y la de Windows.

#include <windows.h>
#include <shellapi.h>

#include <string>

#include <wx/string.h>

#include "../buildinfo.hpp"

namespace pt::UI::Brizna
{
    inline const wchar_t* PaginaEstudio = L"https://brizna-soft.github.io";
    inline const wchar_t* PaginaPrograma = L"https://brizna-soft.github.io/programas/hntorrent/";
    inline const wchar_t* PaginaDonar = L"https://paypal.me/BriznaSoft";
    inline const wchar_t* Repositorio = L"https://github.com/Brizna-Soft/hntorrent";
    // Los reportes van al repositorio de descargas, que es el que tiene los formularios en español
    inline const wchar_t* FormularioFalla = L"https://github.com/Brizna-Soft/hntorrent-descargas/issues/new?template=falla.yml";

    // Solo direcciones web seguras: nunca se abre otra cosa por aquí
    inline void AbrirEnlace(const wxString& url)
    {
        if (!url.StartsWith(L"https://")) return;
        ShellExecuteW(nullptr, L"open", url.wc_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    // Para las direcciones: deja letras, números y «-._~»; lo demás va como %XX (en UTF-8)
    inline wxString Codificar(const wxString& texto)
    {
        wxString r;
        wxScopedCharBuffer utf8 = texto.utf8_str();
        for (size_t i = 0; i < utf8.length(); i++)
        {
            unsigned char c = (unsigned char)utf8.data()[i];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.'
                || c == '_' || c == '~')
                r += (wchar_t)c;
            else
                r += wxString::Format(L"%%%02X", c);
        }
        return r;
    }

    // «Windows 11 (compilación 26300)». Se le pregunta a ntdll porque GetVersionEx miente desde Windows 8.1.
    inline wxString VersionDeWindows()
    {
        using Funcion = LONG(WINAPI*)(OSVERSIONINFOW*);
        OSVERSIONINFOW v{};
        v.dwOSVersionInfoSize = sizeof(v);
        HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
        auto f = ntdll ? reinterpret_cast<Funcion>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion"))) : nullptr;
        if (!f || f(&v) != 0) return L"Windows";
        wxString nombre;
        if (v.dwMajorVersion == 10 && v.dwBuildNumber >= 22000) nombre = L"Windows 11";
        else if (v.dwMajorVersion == 10) nombre = L"Windows 10";
        else nombre = wxString::Format(L"Windows %lu.%lu", v.dwMajorVersion, v.dwMinorVersion);
        return wxString::Format(L"%s (compilación %lu)", nombre, v.dwBuildNumber);
    }

    inline wxString Version()
    {
        return wxString::FromUTF8(pt::BuildInfo::semver());
    }

    // Versión de HNTorrent y de Windows, para pegar en un reporte
    inline wxString DatosParaReporte()
    {
        return L"HNTorrent " + Version() + L" (" + wxString::FromUTF8(pt::BuildInfo::commitish()) + L")\n"
               + VersionDeWindows() + L", 64 bits";
    }

    inline void ReportarProblema()
    {
        AbrirEnlace(wxString(FormularioFalla) + L"&version=" + Codificar(Version()) + L"&windows="
                    + Codificar(VersionDeWindows()));
    }
}
