#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <cwchar>
#include <algorithm>
#include <cstdlib>

// Function to check if the filesystem of the given path is NTFS
bool is_ntfs(const std::wstring& path) {
    std::vector<wchar_t> fs_name(MAX_PATH);
    std::vector<wchar_t> volume_name(MAX_PATH);
    DWORD serial_number = 0;
    DWORD max_component_length = 0;
    DWORD file_system_flags = 0;

    std::wstring root_path;

    size_t colon_pos = path.find(L':');
    if (colon_pos != std::wstring::npos && colon_pos + 1 < path.size()) {
        if (path[colon_pos + 1] != L'\\')
            root_path = path.substr(0, colon_pos + 1) + L"\\";
        else
            root_path = path.substr(0, colon_pos + 2);
    } else {
        root_path = L"C:\\";
    }

    BOOL result = GetVolumeInformationW(
        root_path.c_str(),
        volume_name.data(),
        static_cast<DWORD>(volume_name.size()),
        &serial_number,
        &max_component_length,
        &file_system_flags,
        fs_name.data(),
        static_cast<DWORD>(fs_name.size())
    );

    if (!result) {
        return false;
    }

    size_t fs_name_len = 0;
    while (fs_name_len < fs_name.size() && fs_name[fs_name_len] != L'\0') {
        fs_name_len++;
    }

    std::wstring fs_name_str(fs_name.begin(), fs_name.begin() + fs_name_len);
    std::transform(fs_name_str.begin(), fs_name_str.end(), fs_name_str.begin(), towlower);
    return fs_name_str == L"ntfs";
}

// Function to execute code specific to NTFS filesystems
void execute_ntfs_code() {
    const wchar_t* stream = L":test_stream";
    std::wstring stream_wide(stream);

    size_t rename_info_size = sizeof(FILE_RENAME_INFO) + (stream_wide.size() * sizeof(wchar_t));
    FILE_RENAME_INFO* rename_info = reinterpret_cast<FILE_RENAME_INFO*>(HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        rename_info_size
    ));

    if (rename_info == nullptr) {
        std::cerr << "Memory allocation failed\n";
        std::exit(EXIT_FAILURE);
    }

    rename_info->ReplaceIfExists = TRUE;
    rename_info->RootDirectory = nullptr;
    rename_info->FileNameLength = static_cast<DWORD>(stream_wide.size() * sizeof(wchar_t));
    memcpy(rename_info->FileName, stream_wide.c_str(), stream_wide.size() * sizeof(wchar_t));

    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        std::cerr << "Failed to get current executable path\n";
        HeapFree(GetProcessHeap(), 0, rename_info);
        std::exit(EXIT_FAILURE);
    }

    HANDLE handle = CreateFileW(
        exe_path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (handle == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, rename_info);
        std::cerr << "Failed to open file\n";
        std::exit(EXIT_FAILURE);
    }

    BOOL setfileinfohandle = SetFileInformationByHandle(
        handle,
        FileRenameInfo,
        rename_info,
        static_cast<DWORD>(rename_info_size)
    );

    if (!setfileinfohandle) {
        CloseHandle(handle);
        HeapFree(GetProcessHeap(), 0, rename_info);
        std::cerr << "Failed to set file information for rename\n";
        std::exit(EXIT_FAILURE);
    }

    CloseHandle(handle);

    HANDLE delete_handle = CreateFileW(
        exe_path,
        GENERIC_READ | GENERIC_WRITE | DELETE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE,
        nullptr
    );

    if (delete_handle == INVALID_HANDLE_VALUE) {
        HeapFree(GetProcessHeap(), 0, rename_info);
        std::cerr << "Failed to re-open file for deletion\n";
        std::exit(EXIT_FAILURE);
    }

    FILE_DISPOSITION_INFO delete_file;
    delete_file.DeleteFile = TRUE;

    BOOL setfileinfo = SetFileInformationByHandle(
        delete_handle,
        FileDispositionInfo,
        &delete_file,
        sizeof(FILE_DISPOSITION_INFO)
    );

    if (!setfileinfo) {
        CloseHandle(delete_handle);
        HeapFree(GetProcessHeap(), 0, rename_info);
        std::cerr << "Failed to mark file for deletion\n";
        std::exit(EXIT_FAILURE);
    }

    CloseHandle(delete_handle);
    HeapFree(GetProcessHeap(), 0, rename_info);
}

// Function to execute code for non-NTFS filesystems
void execute_non_ntfs_code() {
    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        std::cerr << "Failed to get current executable path\n";
        std::exit(EXIT_FAILURE);
    }

    std::wstring new_name = L"renamed_file.tmp";
    HANDLE handle = CreateFileW(
        exe_path,
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (handle != INVALID_HANDLE_VALUE) {
        DWORD new_name_length = static_cast<DWORD>((new_name.length() + 1) * sizeof(wchar_t));
        size_t rename_info_size = sizeof(FILE_RENAME_INFO) + (new_name.length() + 1) * sizeof(wchar_t);
        FILE_RENAME_INFO* rename_info = reinterpret_cast<FILE_RENAME_INFO*>(HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            rename_info_size
        ));

        if (rename_info == nullptr) {
            std::cerr << "Memory allocation failed\n";
            CloseHandle(handle);
            std::exit(EXIT_FAILURE);
        }

        rename_info->ReplaceIfExists = TRUE;
        rename_info->RootDirectory = NULL;
        rename_info->FileNameLength = new_name_length;
        memcpy(rename_info->FileName, new_name.c_str(), (new_name.length() + 1) * sizeof(wchar_t));

        BOOL result = SetFileInformationByHandle(
            handle,
            FileRenameInfo,
            rename_info,
            static_cast<DWORD>(rename_info_size)
        );

        if (!result) {
            std::cerr << "Failed to set file information for rename\n";
        }

        CloseHandle(handle);
        HeapFree(GetProcessHeap(), 0, rename_info);
    } else {
        std::cerr << "Failed to open file for renaming\n";
    }
}

int main() {
    wchar_t exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe_path, MAX_PATH);
    if (len == 0 || len == MAX_PATH) {
        std::cerr << "Failed to get current executable path\n";
        return EXIT_FAILURE;
    }

    std::wstring exe_path_str(exe_path);

    if (is_ntfs(exe_path_str)) {
        execute_ntfs_code();
    } else {
        execute_non_ntfs_code();
    }

    return 0;
}
