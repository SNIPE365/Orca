void File_Exit(void) {
    HWND hwnd = _CTL(wcMain);
    MessageBoxA( hwnd , "Quitting From Menu" , g_pzAppName , MB_ICONINFORMATION );
    SendMessage( hwnd , WM_CLOSE , 0 , 0 );
}