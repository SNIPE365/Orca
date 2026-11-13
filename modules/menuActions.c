#include "controlFuncs.c"

void File_Exit(void) {
    HWND hwnd = _CTL(wcMain);
    MessageBoxA( hwnd , "Quitting From Menu" , g_pzAppName , MB_ICONINFORMATION );
    SendMessage( hwnd , WM_CLOSE , 0 , 0 );
}

void View_ToggleRoundStyle(void) {
    menu_MenuState( g_hCurMenu , g_CurItemID , g_CurItemState ? MFS_UNCHECKED : MFS_CHECKED );
    SendMessage( _CTL(wcDiagram) , DIM_SETROUNDSTYLE , !g_CurItemState , 0 );
}

void Project_Build(void) {
    HWND hwnd = _CTL(wcMain);
    ctlParentCommandPost( wcBtnBuild , BN_CLICKED );
}
