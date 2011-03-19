#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "dokan.h"
#include "fileinfo.h"

#include "../fs.h"

BOOL g_UseStdErr;
BOOL g_DebugMode;

void shrink_name(LPCWSTR big_name, char *small_name, size_t max)
{
    wcstombs(small_name, big_name, max);
}

void expand_name(const char *small_name, LPWSTR big_name, size_t max)
{
    mbstowcs(big_name, small_name, max);
}

static void DbgPrint(const char *format, ...)
{
    if (g_DebugMode) {
        char buffer[4096];
        va_list argp;
        va_start(argp, format);
        vsprintf_s(buffer, sizeof(buffer)/sizeof(char), format, argp);
        va_end(argp);
        if (g_UseStdErr) {
            fprintf(stderr, buffer);
        } else {
            printf(buffer);
        }
    }
}


static char filename[MAX_PATH] = "default.fs";
static FS *fs = NULL;


static int is_directory(const char *name)
{
    if (strcmp(name, "\\") == 0)
        return 1;
    return 0;
}


#define CheckFlag(val, flag) if (val&flag) { DbgPrint("\t" #flag "\n"); }

static int __stdcall
OpCreateFile(
    LPCWSTR                    FileName,
    DWORD                    AccessMode,
    DWORD                    ShareMode,
    DWORD                    CreationDisposition,
    DWORD                    FlagsAndAttributes,
    PDOKAN_FILE_INFO        DokanFileInfo)
{
    char name[MAX_PATH];
    FH *fh;

    shrink_name(FileName, name, sizeof(name));

    if (name[0] == '\\' && strlen(name) > 1)
    {
        char temp[MAX_PATH];
        strcpy(temp, name+1);
        strcpy(name, temp);
    }

    DbgPrint("CreateFile : %s\n", name);

    if (CreationDisposition == CREATE_NEW)
        DbgPrint("\tCREATE_NEW\n");
    if (CreationDisposition == OPEN_ALWAYS)
        DbgPrint("\tOPEN_ALWAYS\n");
    if (CreationDisposition == CREATE_ALWAYS)
        DbgPrint("\tCREATE_ALWAYS\n");
    if (CreationDisposition == OPEN_EXISTING)
        DbgPrint("\tOPEN_EXISTING\n");
    if (CreationDisposition == TRUNCATE_EXISTING)
        DbgPrint("\tTRUNCATE_EXISTING\n");

    /*
    if (ShareMode == 0 && AccessMode & FILE_WRITE_DATA)
        ShareMode = FILE_SHARE_WRITE;
    else if (ShareMode == 0)
        ShareMode = FILE_SHARE_READ;
    */

    DbgPrint("\tShareMode = 0x%x\n", ShareMode);

    CheckFlag(ShareMode, FILE_SHARE_READ);
    CheckFlag(ShareMode, FILE_SHARE_WRITE);
    CheckFlag(ShareMode, FILE_SHARE_DELETE);

    DbgPrint("\tAccessMode = 0x%x\n", AccessMode);

    CheckFlag(AccessMode, GENERIC_READ);
    CheckFlag(AccessMode, GENERIC_WRITE);
    CheckFlag(AccessMode, GENERIC_EXECUTE);
    
    CheckFlag(AccessMode, DELETE);
    CheckFlag(AccessMode, FILE_READ_DATA);
    CheckFlag(AccessMode, FILE_READ_ATTRIBUTES);
    CheckFlag(AccessMode, FILE_READ_EA);
    CheckFlag(AccessMode, READ_CONTROL);
    CheckFlag(AccessMode, FILE_WRITE_DATA);
    CheckFlag(AccessMode, FILE_WRITE_ATTRIBUTES);
    CheckFlag(AccessMode, FILE_WRITE_EA);
    CheckFlag(AccessMode, FILE_APPEND_DATA);
    CheckFlag(AccessMode, WRITE_DAC);
    CheckFlag(AccessMode, WRITE_OWNER);
    CheckFlag(AccessMode, SYNCHRONIZE);
    CheckFlag(AccessMode, FILE_EXECUTE);
    CheckFlag(AccessMode, STANDARD_RIGHTS_READ);
    CheckFlag(AccessMode, STANDARD_RIGHTS_WRITE);
    CheckFlag(AccessMode, STANDARD_RIGHTS_EXECUTE);


    // when filePath is a directory, flags is changed to the file be opened
    if (is_directory(name) & FILE_ATTRIBUTE_DIRECTORY) {
        FlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
        //AccessMode = 0;
    }

    DbgPrint("\tFlagsAndAttributes = 0x%x\n", FlagsAndAttributes);

    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ARCHIVE);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ENCRYPTED);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_HIDDEN);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NORMAL);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_OFFLINE);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_READONLY);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_SYSTEM);
    CheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_TEMPORARY);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_WRITE_THROUGH);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_OVERLAPPED);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_NO_BUFFERING);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_RANDOM_ACCESS);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_SEQUENTIAL_SCAN);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_DELETE_ON_CLOSE);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_BACKUP_SEMANTICS);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_POSIX_SEMANTICS);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_REPARSE_POINT);
    CheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_NO_RECALL);
    CheckFlag(FlagsAndAttributes, SECURITY_ANONYMOUS);
    CheckFlag(FlagsAndAttributes, SECURITY_IDENTIFICATION);
    CheckFlag(FlagsAndAttributes, SECURITY_IMPERSONATION);
    CheckFlag(FlagsAndAttributes, SECURITY_DELEGATION);
    CheckFlag(FlagsAndAttributes, SECURITY_CONTEXT_TRACKING);
    CheckFlag(FlagsAndAttributes, SECURITY_EFFECTIVE_ONLY);
    CheckFlag(FlagsAndAttributes, SECURITY_SQOS_PRESENT);

    if (strcmp(name, "\\") == 0)
    {
        DokanFileInfo->Context = 0;
        return 0;
    }

    if (!fs)
    {
        if (CreationDisposition == OPEN_ALWAYS || CreationDisposition == OPEN_EXISTING)
            return -ERROR_FILE_NOT_FOUND;

        if (CreationDisposition == CREATE_ALWAYS || CreationDisposition == CREATE_NEW)
            return -ERROR_WRITE_PROTECT;

        return -1;
    }

    if (CreationDisposition == OPEN_EXISTING && !file_exists(fs, name))
        return -ERROR_FILE_NOT_FOUND;

    if ((CreationDisposition == CREATE_NEW || CreationDisposition == CREATE_ALWAYS) && !file_exists(fs, name))
    {
        fh = fs_open_file(fs, name);
        DokanFileInfo->Context = (ULONG64) fh;
        return 0;
    }

    if ((CreationDisposition == OPEN_ALWAYS || CreationDisposition == OPEN_EXISTING) && file_exists(fs, name))
    {
        DbgPrint("\tHello = 0x%x\n", FlagsAndAttributes);
        fh = fs_open_file(fs, name);
        DokanFileInfo->Context = (ULONG64) fh;
        return 0;
    }

    return -1;

#if 0
    handle = CreateFile(
        filePath,
        AccessMode,//GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
        ShareMode,
        NULL, // security attribute
        CreationDisposition,
        FlagsAndAttributes,// |FILE_FLAG_NO_BUFFERING,
        NULL); // template file handle

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        DbgPrint("\terror code = %d\n\n", error);
        return error * -1; // error codes are negated value of Windows System Error codes
    }

    DbgPrint("\n");

    // save the file handle in Context
    DokanFileInfo->Context = (ULONG64)handle;
    return 0;
#endif
}


static int __stdcall
OpCreateDirectory(
    LPCWSTR                    FileName,
    PDOKAN_FILE_INFO        DokanFileInfo)
{
    char name[MAX_PATH];
    shrink_name(FileName, name, sizeof(name));

    DbgPrint("CreateDirectory : %s\n", name);

    return -1;
#if 0
    if (!CreateDirectory(filePath, NULL)) {
        DWORD error = GetLastError();
        DbgPrint("\terror code = %d\n\n", error);
        return error * -1; // error codes are negated value of Windows System Error codes
    }
    return 0;
#endif
}


static int __stdcall
OpOpenDirectory(
    LPCWSTR                    FileName,
    PDOKAN_FILE_INFO        DokanFileInfo)
{
    char name[MAX_PATH];

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("OpenDirectory : %s\n", name);

    return 0;

#if 0
    handle = CreateFile(
        filePath,
        0,
        FILE_SHARE_READ|FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL);

    if (handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        DbgPrint("\terror code = %d\n\n", error);
        return error * -1;
    }

    DbgPrint("\n");

    DokanFileInfo->Context = (ULONG64)handle;

    return 0;
#endif
}


static int __stdcall
OpCloseFile(
    LPCWSTR                    FileName,
    PDOKAN_FILE_INFO        DokanFileInfo)
{
    char name[MAX_PATH];
    FH *fh;

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("CloseFile : %s\n", name);

    if (!fs)
        return 0;

    fh = (FH *) DokanFileInfo->Context;
    if (fh)
        fs_close_file(fs, fh);
    return 0;

#if 0
    if (DokanFileInfo->Context) {
        DbgPrint("CloseFile: %s\n", filePath);
        DbgPrint("\terror : not cleanuped file\n\n");
        CloseHandle((HANDLE)DokanFileInfo->Context);
        DokanFileInfo->Context = 0;
    } else {
        //DbgPrint("Close: %s\n\tinvalid handle\n\n", filePath);
        DbgPrint("Close: %s\n\n", filePath);
        return 0;
    }

    //DbgPrint("\n");
    return 0;
#endif
}


static int __stdcall
OpCleanup(
    LPCWSTR                    FileName,
    PDOKAN_FILE_INFO        DokanFileInfo)
{
    char name[MAX_PATH];
    shrink_name(FileName, name, sizeof(name));

    return 0;

#if 0
    if (DokanFileInfo->Context) {
        DbgPrint("Cleanup: %s\n\n", filePath);
        CloseHandle((HANDLE)DokanFileInfo->Context);
        DokanFileInfo->Context = 0;

        if (DokanFileInfo->DeleteOnClose) {
            if (DokanFileInfo->IsDirectory) {
                DbgPrint("  DeleteDirectory ");
                if (!RemoveDirectory(filePath)) {
                    DbgPrint("error code = %d\n\n", GetLastError());
                } else {
                    DbgPrint("success\n\n");
                }
            } else {
                DbgPrint("  DeleteFile ");
                if (DeleteFile(filePath) == 0) {
                    DbgPrint(" error code = %d\n\n", GetLastError());
                } else {
                    DbgPrint("success\n\n");
                }
            }
        }


    } else {
        DbgPrint("Cleanup: %s\n\tinvalid handle\n\n", filePath);
        return -1;
    }

    return 0;
#endif
}


static int __stdcall
OpReadFile(
    LPCWSTR                FileName,
    LPVOID                Buffer,
    DWORD                BufferLength,
    LPDWORD                ReadLength,
    LONGLONG            Offset,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];
    HANDLE    handle = (HANDLE)DokanFileInfo->Context;
    ULONG    offset = (ULONG)Offset;
    BOOL    opened = FALSE;
    FH *fh;

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("ReadFile : %s\n", name);

    if (!fs)
        return -1;

    fh = (FH *) DokanFileInfo->Context;
    *ReadLength = read_data(fs, fh, (int) Offset, Buffer, BufferLength);

    return 0;

#if 0
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle, cleanuped?\n");
        handle = CreateFile(
            filePath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if (handle == INVALID_HANDLE_VALUE) {
            DbgPrint("\tCreateFile error : %d\n\n", GetLastError());
            return -1;
        }
        opened = TRUE;
    }
    
    if (SetFilePointer(handle, offset, NULL, FILE_BEGIN) == 0xFFFFFFFF) {
        DbgPrint("\tseek error, offset = %d\n\n", offset);
        if (opened)
            CloseHandle(handle);
        return -1;
    }

        
    if (!ReadFile(handle, Buffer, BufferLength, ReadLength,NULL)) {
        DbgPrint("\tread error = %u, buffer length = %d, read length = %d\n\n",
            GetLastError(), BufferLength, *ReadLength);
        if (opened)
            CloseHandle(handle);
        return -1;

    } else {
        DbgPrint("\tread %d, offset %d\n\n", *ReadLength, offset);
    }

    if (opened)
        CloseHandle(handle);

    return 0;
#endif
}


static int __stdcall
OpWriteFile(
    LPCWSTR        FileName,
    LPCVOID        Buffer,
    DWORD        NumberOfBytesToWrite,
    LPDWORD        NumberOfBytesWritten,
    LONGLONG            Offset,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];
    HANDLE    handle = (HANDLE)DokanFileInfo->Context;
    ULONG    offset = (ULONG)Offset;
    BOOL    opened = FALSE;
    FH *fh;

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("WriteFile : %s, offset %I64d, length %d\n", name, Offset, NumberOfBytesToWrite);
    //DbgPrint("----\n%s\n----\n\n", Buffer);

    //DbgPrint("press any key?");
    //getchar();

    if (!fs)
        return -1;

    fh = (FH *) DokanFileInfo->Context;
    write_data(fs, fh, (int) Offset, Buffer, NumberOfBytesToWrite);
    *NumberOfBytesWritten = NumberOfBytesToWrite;

    return 0;

#if 0
    // reopen the file
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle, cleanuped?\n");
        handle = CreateFile(
            filePath,
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL);
        if (handle == INVALID_HANDLE_VALUE) {
            DbgPrint("\tCreateFile error : %d\n\n", GetLastError());
            return -1;
        }
        opened = TRUE;
    }

    if (SetFilePointer(handle, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        DbgPrint("\tseek error, offset = %d, error = %d\n", offset, GetLastError());
        return -1;
    }

        
    if (!WriteFile(handle, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, NULL)) {
        DbgPrint("\twrite error = %u, buffer length = %d, write length = %d\n",
            GetLastError(), NumberOfBytesToWrite, *NumberOfBytesWritten);
        return -1;

    } else {
        DbgPrint("\twrite %d, offset %d\n\n", *NumberOfBytesWritten, offset);
    }

    // close the file when it is reopened
    if (opened)
        CloseHandle(handle);

    return 0;
#endif
}


static int __stdcall
OpFlushFileBuffers(
    LPCWSTR        FileName,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];
    HANDLE    handle = (HANDLE)DokanFileInfo->Context;

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("FlushFileBuffers : %s\n", name);

    if (!fs)
        return -1;

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle\n\n");
        return 0;
    }

    if (FlushFileBuffers(handle)) {
        return 0;
    } else {
        DbgPrint("\tflush error code = %d\n", GetLastError());
        return -1;
    }

}


static int __stdcall
OpGetFileInformation(
    LPCWSTR                            FileName,
    LPBY_HANDLE_FILE_INFORMATION    HandleFileInformation,
    PDOKAN_FILE_INFO                DokanFileInfo)
{
    char name[MAX_PATH];
    HANDLE    handle = (HANDLE)DokanFileInfo->Context;
    BOOL    opened = FALSE;

    shrink_name(FileName, name, sizeof(name));

    if (name[0] == '\\' && strlen(name) > 1)
    {
        char temp[MAX_PATH];
        strcpy(temp, name+1);
        strcpy(name, temp);
    }

    DbgPrint("GetFileInfo : %s\n", name);

    if (is_directory(name))
        HandleFileInformation->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    else
    {
        HandleFileInformation->dwFileAttributes = 0;
        HandleFileInformation->nFileSizeHigh = 0;
        HandleFileInformation->nFileSizeLow = get_file_size(fs, name);
    }

    return 0;
#if 0
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle\n\n");

        // If CreateDirectory returned FILE_ALREADY_EXISTS and 
        // it is called with FILE_OPEN_IF, that handle must be opened.
        handle = CreateFile(filePath, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (handle == INVALID_HANDLE_VALUE)
            return -1;
        opened = TRUE;
    }

    if (!GetFileInformationByHandle(handle,HandleFileInformation)) {
        DbgPrint("\terror code = %d\n", GetLastError());

        // FileName is a root directory
        // in this case, FindFirstFile can't get directory information
        if (wcslen(FileName) == 1) {
            HandleFileInformation->dwFileAttributes = GetFileAttributes(filePath);

        } else {
            WIN32_FIND_DATAW find;
            ZeroMemory(&find, sizeof(WIN32_FIND_DATAW));
            handle = FindFirstFile(filePath, &find);
            if (handle == INVALID_HANDLE_VALUE) {
                DbgPrint("\tFindFirstFile error code = %d\n\n", GetLastError());
                return -1;
            }
            HandleFileInformation->dwFileAttributes = find.dwFileAttributes;
            HandleFileInformation->ftCreationTime = find.ftCreationTime;
            HandleFileInformation->ftLastAccessTime = find.ftLastAccessTime;
            HandleFileInformation->ftLastWriteTime = find.ftLastWriteTime;
            HandleFileInformation->nFileSizeHigh = find.nFileSizeHigh;
            HandleFileInformation->nFileSizeLow = find.nFileSizeLow;
            DbgPrint("\tFindFiles OK\n");
            FindClose(handle);
        }
    }

    DbgPrint("\n");

    if (opened) {
        CloseHandle(handle);
    }

    return 0;
#endif
}


struct findfiles_baton
{
    PFillFindData        FillFindData;
    PDOKAN_FILE_INFO    DokanFileInfo;
};


/*
typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD nFileSizeHigh;
    DWORD nFileSizeLow;
    DWORD dwReserved0;
    DWORD dwReserved1;
    WCHAR  cFileName[ MAX_PATH ];
    WCHAR  cAlternateFileName[ 14 ];
} WIN32_FIND_DATAW, *PWIN32_FIND_DATAW, *LPWIN32_FIND_DATAW;
*/
static int findfiles_callback(const char *name, void *baton)
{
    struct findfiles_baton *ffb = (struct findfiles_baton *) baton;
    WIN32_FIND_DATAW find_data;

    memset(&find_data, 0, sizeof(find_data));

    find_data.dwFileAttributes = is_directory(name) ? FILE_ATTRIBUTE_DIRECTORY : 0;
    find_data.nFileSizeHigh = 0;
    find_data.nFileSizeLow = get_file_size(fs, name);
    expand_name(name, find_data.cFileName, strlen(name));

    ffb->FillFindData(&find_data, ffb->DokanFileInfo);

    return 0;
}


static int __stdcall
OpFindFiles(
    LPCWSTR                FileName,
    PFillFindData        FillFindData, // function pointer
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];
    struct findfiles_baton ffb;

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("FindFiles :%s\n", name);

    if (!fs)
        return -1;

    if (name[0] == '\\' && strlen(name) > 1)
    {
        char temp[MAX_PATH];
        strcpy(temp, name+1);
        strcpy(name, temp);
    }

    ffb.FillFindData = FillFindData;
    ffb.DokanFileInfo = DokanFileInfo;
    list_fs(fs, name, findfiles_callback, &ffb);

    return 0;
#if 0
    hFind = FindFirstFile(filePath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid file handle. Error is %u\n\n", GetLastError());
        return -1;
    }

    FillFindData(&findData, DokanFileInfo);
    count++;

    while (FindNextFile(hFind, &findData) != 0) {
         FillFindData(&findData, DokanFileInfo);
        count++;
    }
    
    error = GetLastError();
    FindClose(hFind);

    if (error != ERROR_NO_MORE_FILES) {
        DbgPrint("\tFindNextFile error. Error is %u\n\n", error);
        return -1;
    }

    DbgPrint("\tFindFiles return %d entries in %s\n\n", count, filePath);

    return 0;
#endif
}


static int __stdcall
OpDeleteFile(
    LPCWSTR                FileName,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];
    HANDLE    handle = (HANDLE)DokanFileInfo->Context;

    shrink_name(FileName, name, sizeof(name));

    if (name[0] == '\\' && strlen(name) > 1)
    {
        char temp[MAX_PATH];
        strcpy(temp, name+1);
        strcpy(name, temp);
    }

    DbgPrint("DeleteFile %s\n", name);

    if (!file_exists(fs, name))
        return -ERROR_FILE_NOT_FOUND;

    if (!fs_delete_file(fs, name))
        return -1;

    DokanFileInfo->Context = 0;

    return 0;
}


static int __stdcall
OpDeleteDirectory(
    LPCWSTR                FileName,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("DeleteDirectory %s\n", name);

    return -1;
#if 0
    hFind = FindFirstFile(filePath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_NO_MORE_FILES) {
            return 0;
        } else {
            DbgPrint("\tinvalid file handle. Error is %u\n\n", GetLastError());
            return -1;
        }
    } else {
        FindClose(hFind);
        return -(int)STATUS_DIRECTORY_NOT_EMPTY;
    }
    
    return 0;
#endif
}


static int __stdcall
OpMoveFile(
    LPCWSTR                FileName, // existing file name
    LPCWSTR                NewFileName,
    BOOL                ReplaceIfExisting,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];
    char new_name[MAX_PATH];

    shrink_name(FileName, name, sizeof(name));
    shrink_name(NewFileName, new_name, sizeof(new_name));

    DbgPrint("MoveFile %s -> %s\n\n", name, new_name);

    return -1;
#if 0
    if (DokanFileInfo->Context) {
        // should close? or rename at closing?
        CloseHandle((HANDLE)DokanFileInfo->Context);
        DokanFileInfo->Context = 0;
    }

    if (ReplaceIfExisting)
        status = MoveFileEx(filePath, newFilePath, MOVEFILE_REPLACE_EXISTING);
    else
        status = MoveFile(filePath, newFilePath);

    if (status == FALSE) {
        DWORD error = GetLastError();
        DbgPrint("\tMoveFile failed status = %d, code = %d\n", status, error);
        return -(int)error;
    } else {
        return 0;
    }
#endif
}


static int __stdcall
OpLockFile(
    LPCWSTR                FileName,
    LONGLONG            ByteOffset,
    LONGLONG            Length,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("LockFile %s\n", name);

    return -1;
#if 0
    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle\n\n");
        return -1;
    }

    length.QuadPart = Length;
    offset.QuadPart = ByteOffset;

    if (LockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
        DbgPrint("\tsuccess\n\n");
        return 0;
    } else {
        DbgPrint("\tfail\n\n");
        return -1;
    }
#endif
}


static int __stdcall
OpSetEndOfFile(
    LPCWSTR                FileName,
    LONGLONG            ByteOffset,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("SetEndOfFile %s, %I64d\n", name, ByteOffset);

    return -1;
#if 0
    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle\n\n");
        return -1;
    }

    offset.QuadPart = ByteOffset;
    if (!SetFilePointerEx(handle, offset, NULL, FILE_BEGIN)) {
        DbgPrint("\tSetFilePointer error: %d, offset = %I64d\n\n", GetLastError(), ByteOffset);
        return GetLastError() * -1;
    }

    if (!SetEndOfFile(handle)) {
        DWORD error = GetLastError();
        DbgPrint("\terror code = %d\n\n", error);
        return error * -1;
    }

    return 0;
#endif
}


static int __stdcall
OpSetFileAttributes(
    LPCWSTR                FileName,
    DWORD                FileAttributes,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];
    
    shrink_name(FileName, name, sizeof(name));

    DbgPrint("SetFileAttributes %s\n", name);

    return -1;
#if 0
    if (!SetFileAttributes(filePath, FileAttributes)) {
        DWORD error = GetLastError();
        DbgPrint("\terror code = %d\n\n", error);
        return error * -1;
    }

    DbgPrint("\n");
    return 0;
#endif
}


static int __stdcall
OpSetFileTime(
    LPCWSTR                FileName,
    CONST FILETIME*        CreationTime,
    CONST FILETIME*        LastAccessTime,
    CONST FILETIME*        LastWriteTime,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("SetFileTime %s\n", name);

    return -1;
#if 0
    handle = (HANDLE)DokanFileInfo->Context;

    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle\n\n");
        return -1;
    }

    if (!SetFileTime(handle, CreationTime, LastAccessTime, LastWriteTime)) {
        DWORD error = GetLastError();
        DbgPrint("\terror code = %d\n\n", error);
        return error * -1;
    }

    DbgPrint("\n");
    return 0;
#endif
}



static int __stdcall
OpUnlockFile(
    LPCWSTR                FileName,
    LONGLONG            ByteOffset,
    LONGLONG            Length,
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    char name[MAX_PATH];

    shrink_name(FileName, name, sizeof(name));

    DbgPrint("UnlockFile %s\n", name);

    return -1;
#if 0
    handle = (HANDLE)DokanFileInfo->Context;
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        DbgPrint("\tinvalid handle\n\n");
        return -1;
    }

    length.QuadPart = Length;
    offset.QuadPart = ByteOffset;

    if (UnlockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
        DbgPrint("\tsuccess\n\n");
        return 0;
    } else {
        DbgPrint("\tfail\n\n");
        return -1;
    }
#endif
}


static int __stdcall
OpUnmount(
    PDOKAN_FILE_INFO    DokanFileInfo)
{
    DbgPrint("Unmount\n");

    if (!fs)
        return -1;

    flush_fs(fs);
    return 0;
}


static DOKAN_OPERATIONS
dokanOperations = {
    OpCreateFile,
    OpOpenDirectory,
    OpCreateDirectory,
    OpCleanup,
    OpCloseFile,
    OpReadFile,
    OpWriteFile,
    OpFlushFileBuffers,
    OpGetFileInformation,
    OpFindFiles,
    NULL, // FindFilesWithPattern
    OpSetFileAttributes,
    OpSetFileTime,
    OpDeleteFile,
    OpDeleteDirectory,
    OpMoveFile,
    OpSetEndOfFile,
    OpLockFile,
    OpUnlockFile,
    NULL, // GetDiskFreeSpace
    NULL, // GetVolumeInformation
    OpUnmount // Unmount
};



int __cdecl
main(ULONG argc, PCHAR argv[])
{
    int status;
    ULONG command;
    PDOKAN_OPTIONS dokanOptions = (PDOKAN_OPTIONS)malloc(sizeof(DOKAN_OPTIONS));

    //return test_main(argc, argv);

    if (argc < 5) {
        fprintf(stderr, "fs.exe\n"
            "  /f Filename (ex. /f c:\\test.fs)\n"
            "  /l DriveLetter (ex. /l m)\n"
            "  /t ThreadCount (ex. /t 5)\n"
            "  /d (enable debug output)\n"
            "  /s (use stderr for output)");
        return -1;
    }

    g_DebugMode = FALSE;
    g_UseStdErr = FALSE;

    ZeroMemory(dokanOptions, sizeof(DOKAN_OPTIONS));
    dokanOptions->ThreadCount = 0; // use default

    for (command = 1; command < argc; command++) {
        switch (tolower(argv[command][1])) {
        case 'f':
            command++;
            strcpy(filename, argv[command]);
            DbgPrint("Filename: %s\n", filename);
            break;
        case 'l':
            command++;
            dokanOptions->DriveLetter = argv[command][0];
            break;
        case 't':
            command++;
            dokanOptions->ThreadCount = (USHORT)atoi(argv[command]);
            break;
        case 'd':
            g_DebugMode = TRUE;
            break;
        case 's':
            g_UseStdErr = TRUE;
            break;
        default:
            fprintf(stderr, "unknown command: %s\n", argv[command]);
            return -1;
        }
    }

    dokanOptions->DebugMode = (UCHAR)g_DebugMode;
    dokanOptions->UseStdErr = (UCHAR)g_UseStdErr;
    dokanOptions->UseKeepAlive = 1;

    //fs_watch_mode = 1;
    fs = open_fs(filename);

    status = DokanMain(dokanOptions, &dokanOperations);
    if (fs)
        flush_fs(fs);
    switch (status) {
        case DOKAN_SUCCESS:
            fprintf(stderr, "Success\n");
            break;
        case DOKAN_ERROR:
            fprintf(stderr, "Error\n");
            break;
        case DOKAN_DRIVE_LETTER_ERROR:
            fprintf(stderr, "Bad Drive letter\n");
            break;
        case DOKAN_DRIVER_INSTALL_ERROR:
            fprintf(stderr, "Can't install driver\n");
            break;
        case DOKAN_START_ERROR:
            fprintf(stderr, "Driver something wrong\n");
            break;
        case DOKAN_MOUNT_ERROR:
            fprintf(stderr, "Can't assign a drive letter\n");
            break;
        default:
            fprintf(stderr, "Unknown error: %d\n", status);
            break;
    }

    return 0;
}
