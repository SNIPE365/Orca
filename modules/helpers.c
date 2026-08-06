static void defines () { //defines
    #define STRINGIFY(x) #x
    #define TOSTRING(x) STRINGIFY(x)
    #define _err( _msg... ) static_assert( 0 , _msg )

    #define _auto __auto_type
    #define _const const __auto_type
    #define _with(_var) { _const w = &_var;
    #define _endwith }    

    #if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202000L)    
        #define _constexpr( _parms... ) constexpr __auto_type _parms
    #else
        #define _constexpr( _parms... ) enum { _parms }
    #endif
    //_constexpr( test = 100 ); _constexpr( test2 = 110 );
    
    //#define _countof( _var ) (sizeof((*_var))/sizeof(typeof(*(_var)))
    
    #define SWAP(_a, _b) do { \
        typeof(_a) _temp = (_a); \
        (_a) = (_b); \
        (_b) = _temp; \
    } while(0)
    
}

// get size of a file without opening it.
uint64_t GetFileSize64(const char* filePath) {
    WIN32_FILE_ATTRIBUTE_DATA fileInfo;    
    // GetFileExInfoStandard is the standard level of information
    if (GetFileAttributesExA(filePath, GetFileExInfoStandard, &fileInfo)) {        
        ULARGE_INTEGER size;
        size.HighPart = fileInfo.nFileSizeHigh;
        size.LowPart  = fileInfo.nFileSizeLow;        
        return size.QuadPart;
    }    
    return (uint64_t)-1; // File not found or access denied
}

// Executes a command using CreateProcess, capturing STDOUT and STDERR.
// timeout_ms: Maximum time to wait in milliseconds. Use 0 for infinite wait.
// Returns the length of the string, or -1 on failure.
int ReadProcessOutput(const char* pzCommand, char** ppzOut_buffer , DWORD* piReturnCode , const int iTimeout_ms ) {
    
    if (piReturnCode) { *piReturnCode = 0; }
    if (!pzCommand || !ppzOut_buffer) { return -1; }
    *ppzOut_buffer = NULL;

    // 1. Create a pipe for the child process's STDOUT/STDERR
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE; // The child MUST inherit the write end
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        return -1;
    }

    // CRITICAL: Ensure the read handle to the pipe is NOT inherited by the child.
    // If it is inherited, the pipe will never close and we will hang indefinitely.
    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }

    // 2. Set up StartupInfo
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    // STARTF_USESTDHANDLES tells it to use our pipes.
    // STARTF_USESHOWWINDOW combined with SW_HIDE acts as a fallback to hide the window.
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    // CreateProcessA requires a mutable string for the command line.
    // If we pass a const char* string literal, it can cause an Access Violation.
    char* cmd_mutable = (char*)malloc(strlen(pzCommand) + 1);
    strcpy(cmd_mutable, pzCommand);

    // 3. Create the child process
    BOOL bSuccess = CreateProcessA(
        NULL, 
        cmd_mutable,       // Mutable command line 
        NULL,              // Process security attributes 
        NULL,              // Primary thread security attributes 
        TRUE,              // Handles are inherited 
        CREATE_NO_WINDOW,  // Creation flags (prevents console from appearing)
        NULL,              // Use parent's environment 
        NULL,              // Use parent's current directory 
        &si,               // STARTUPINFO pointer 
        &pi                // Receives PROCESS_INFORMATION 
    );
    
    free(cmd_mutable);

    if (!bSuccess) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return -1;
    }

    // CRITICAL: Close the parent's copy of the write end of the pipe!
    // If we don't do this, the pipe stays open even after the child exits.
    CloseHandle(hWritePipe);

    // 4. Read the output with a timeout
    int capacity = 4096;
    int length = 0;
    char* buffer = (char*)malloc(capacity);
    if (!buffer) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
        return -1;
    }

    DWORD startTime = GetTickCount();

    for (;;) {
        // Enforce Timeout
        if (iTimeout_ms > 0 && (GetTickCount() - startTime > iTimeout_ms)) {
            TerminateProcess(pi.hProcess, 1); // Kill hanging process
            break;
        }

        DWORD bytesAvailable = 0;
        
        // PeekNamedPipe checks if there is data without blocking execution
        BOOL bPeek = PeekNamedPipe(hReadPipe, NULL, 0, NULL, &bytesAvailable, NULL);
        
        if (bPeek && bytesAvailable > 0) {
            // Ensure we have enough space for the available bytes + null terminator
            if (length + bytesAvailable >= capacity - 1) {
                capacity = capacity + bytesAvailable + 4096; 
                char* new_buf = (char*)realloc(buffer, capacity);
                if (!new_buf) break; // Memory error, stop reading
                buffer = new_buf;
            }
            
            DWORD bytesRead = 0;
            if (ReadFile(hReadPipe, buffer + length, bytesAvailable, &bytesRead, NULL)) {
                length += bytesRead; if (iTimeout_ms>0) { startTime = GetTickCount(); }
            } else {
                break; // Error reading pipe
            }
        } else {
            // No data currently in pipe. Check if child process has exited.
            if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) {
                // Process is dead and there is no more data in the pipe.
                break;
            }
            
            // Sleep briefly to yield CPU thread time while waiting for child to output
            Sleep(10);
        }
    }

    // 5. Cleanup and return
    buffer[length] = '\0';
    *ppzOut_buffer = buffer;
    
    if (piReturnCode) GetExitCodeProcess( pi.hProcess , piReturnCode );    

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);

    return length;
}