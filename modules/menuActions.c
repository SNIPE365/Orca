void File_Exit(void) {
    HWND hwnd = g_CTL[wcMain];
    MessageBoxA( hwnd , "Quitting From Menu" , g_pzAppName , MB_ICONINFORMATION );
    SendMessage( hwnd , WM_CLOSE , 0 , 0 );
}