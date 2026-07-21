#define main_module Orca

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windows.h>
#include <commctrl.h>
//#include <uxtheme.h>

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
}

HINSTANCE g_APPINSTANCE;  //instance
HMENU g_WndMenu;          //menu
//AppName
char* g_pzAppName = "Orca IDE";      

UINT g_CurItemID=0 , g_CurItemState=0; 
HMENU g_hCurMenu=NULL;
#define _Wnd MAIN

#include "modules\wndCreate.c"
#include "modules\menu.c"

HRESULT (*fnSetWindowTheme) (HWND,LPCWSTR,LPCWSTR) = NULL;
  
DWORD g_dwEnableSizeBorder = 0;
static void UpdateSizeBorder( HWND hwnd , DWORD dwStyle ) {
    RECT rc ; GetWindowRect( hwnd , &rc );
    int iFrameSz = GetSystemMetrics( SM_CXFRAME )-3;                            
    if ((dwStyle & WS_THICKFRAME)) { iFrameSz = -iFrameSz; }
    rc.left  += iFrameSz; rc.top    += iFrameSz;
    rc.right -= iFrameSz; rc.bottom -= iFrameSz;
    SetWindowLong( hwnd , GWL_STYLE , dwStyle );
    _const cSwp = SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED;
    //SetWindowPos( hwnd , NULL , rc.left,rc.top , rc.right-rc.left,rc.bottom-rc.top , cSwp );
                                
    HDWP pDefer = BeginDeferWindowPos( 1 );
    DeferWindowPos( pDefer , hwnd , NULL , rc.left,rc.top , rc.right-rc.left,rc.bottom-rc.top , cSwp );
    EndDeferWindowPos( pDefer );
}    

// *************** Procedure Function ****************
static CALLBACK LRESULT WndProc ( HWND hwnd , UINT message, WPARAM wparam, LPARAM lparam ) {  
    switch ( message ) {
        case WM_CREATE:  { //Window was created
            return wndCreate( hwnd );
        } // WM_CREATE
        case WM_COMMAND: { //Event happened to a control (child window/control)
            _auto wNotifyCode = (int)(HIWORD(wparam));
            _const wID = LOWORD(wparam);
            _const hwndCtl = (HWND)lparam;
            if (!hwndCtl && !wNotifyCode) { wNotifyCode = -1; }
            switch (wNotifyCode) {
                case -1:         { //Command from the menu                    
                    if (wID != g_CurItemID) { return 0; } //not valid menu event
                    MENUITEMINFO tItem = { sizeof(MENUITEMINFO) , MIIM_DATA | MIIM_STATE };
                    GetMenuItemInfo( g_hCurMenu , wID , false , &tItem );
                    g_CurItemState = tItem.fState;
                    if (tItem.dwItemData) {                        
                        void (*MenuItemCallback)(void) = (void (*)(void))tItem.dwItemData;
                        MenuItemCallback();
                    }
                    g_hCurMenu = NULL;
                    return g_CurItemID = 0; //break
                }
                case 1:          { //Accelerator
                    //ProcessAccelerator( wID )
                    return 0;
                }
                case EN_CHANGE:  {
                    printf("%i\n,",SendMessage(_CTL(wID),WM_GETTEXTLENGTH,0,0));
                } //EN_CHANGE
                case BN_CLICKED: { //button click            
                    switch (LOWORD(wparam)) {
                        case wcBtnCmd: {
                            MessageBox( hwnd , "Bye" , "Bye" , MB_ICONINFORMATION );
                            PostQuitMessage(0);                            
                            break;
                        } //wcButton
                    } // switch (lparam)
                } //BN_CLICKED
            } //switch HIWORD(wparam)
            return 0;
        }
        case WM_TIMER: {
            if (wparam != WM_NCMOUSEMOVE) { break; }
            KillTimer( hwnd , wparam );
            POINT pt; GetCursorPos( &pt );
            RECT rc; GetWindowRect( hwnd , &rc );
            if (PtInRect( &rc , pt )) { 
                if (g_dwEnableSizeBorder) { UpdateSizeBorder( hwnd , g_dwEnableSizeBorder ) ; g_dwEnableSizeBorder = 0; }
                break; 
            } //may falltrough
        }
        case WM_MOUSEMOVE: {
            wparam = HTCLIENT;
            //falltrough
        }
        case WM_NCMOUSEMOVE: {            
            bool bEnabled = false , bChanged = false;
            switch ( wparam ) {
                case HTBORDER : case HTBOTTOM : case HTBOTTOMLEFT : case HTBOTTOMRIGHT : case HTGROWBOX :
                case HTLEFT : case HTRIGHT : case HTTOP : case HTTOPLEFT : case HTTOPRIGHT : bEnabled = true;
            }
            DWORD newStyle = GetWindowLong( hwnd , GWL_STYLE ) ^ WS_THICKFRAME;            
            if ((bEnabled) && ((newStyle & WS_THICKFRAME))) { bChanged = true; }
            if ((!bEnabled) && (!(newStyle & WS_THICKFRAME))) { bChanged = true; }
            //printf("enabled=%i changed=%i\n",bEnabled,bChanged);
            if (bEnabled && g_dwEnableSizeBorder) { SetTimer( hwnd , WM_NCMOUSEMOVE , 100 , NULL ); break; } //do nothing while we have a delayed update            
            if (bChanged) { 
              if (bEnabled) { g_dwEnableSizeBorder = newStyle; } else { g_dwEnableSizeBorder=0 ; UpdateSizeBorder( hwnd , newStyle ); }
            }
            if (bEnabled) { SetTimer( hwnd , WM_NCMOUSEMOVE , 200 , NULL ); }
            break;
        }
        case WM_SIZE:    {
            wndResize( hwnd );
            //puts("Size changed?");            
            //printf("%ix%i\n", g_tMain.iW , g_tMain.iH);
            break;
        }
        case WM_MENUSELECT: { //track newest menu handle/item/state
            _const iID = (UINT)(LOWORD(wparam)); 
            _const fuFlags = (UINT)(HIWORD(wparam)); 
            _const hMenu = (HMENU)lparam; 
            if (hMenu) { g_CurItemID = iID ; g_hCurMenu = hMenu; }
            return 0; //break;
        } //WM_MENUSELECT
        case WM_NOTIFY:  { //some events goes trough notify instead of command (child window/control)
            break;
        } // WM_NOTIFY
        case WM_PAINT:   { //this is called when an area of the window need to be painted (so manual drawing can be done here)
            /*
              var hDC = GetDC(hwnd)
              DrawStatusText( hDC , @type<RECT>(200,8,300,32) , "Hello World" , 0 
              DrawState( hDC , NULL , NULL , cast(LPARAM,@"Hello WOrld") , 0 , 200,8 , 0,0 , DST_TEXT or DSS_DISABLED )
              ReleaseDC( hwnd , hDC )
              return 0
            */
            break;
        } //WM_PAINT
        case WM_DESTROY: { //'Windows was closed/destroyed
            PostQuitMessage(0); //to quit
            return 0;
        } //WM_DESTROY
    } // switch ( message )
  
    // *** if program reach here default predefined action will happen ***
    return DefWindowProc( hwnd, message, wparam, lparam );    
    
} //WndProc()

#undef _CTL

/* *********************************************************************
   *********************** SETUP MAIN WINDOW ***************************
   ******************* This code can be ignored ************************
   ********************************************************************* */
int main() {

    _auto hUxTheme = LoadLibraryA("uxtheme.dll");
    if (hUxTheme) {
        fnSetWindowTheme = (void*)GetProcAddress( hUxTheme , "SetWindowTheme" );
    }
    
    InitCommonControls();
    g_APPINSTANCE = GetModuleHandle(NULL);
  
    MSG wMsg = {0};
    WNDCLASS wcls = {0};
    HWND hwnd;

    // Setup window class  
    _with(wcls) {
        w->style         = CS_HREDRAW | CS_VREDRAW;
        w->lpfnWndProc   = WndProc;
        w->cbClsExtra    = 0;
        w->cbWndExtra    = 0;
        w->hInstance     = g_APPINSTANCE;
        w->hIcon         = LoadIcon( g_APPINSTANCE, "FB_PROGRAM_ICON" );
        w->hCursor       = LoadCursor( NULL, IDC_ARROW );
        w->hbrBackground = GetSysColorBrush( COLOR_BTNFACE );
        w->lpszMenuName  = NULL;
        w->lpszClassName = g_pzAppName;
    } _endwith

    // Register the window class     
    if ( !RegisterClass( &wcls ) ) {
        MessageBoxA( NULL, "Failed to register wcls!", g_pzAppName, MB_ICONINFORMATION );
        return 1;
    }
    
    g_WndMenu = menu_CreateMainMenu();
        
    // Create the window and show it  
    _const cStyleEx = 0; //WS_EX_COMPOSITED | WS_EX_LAYERED;
    _const cStyle   = (WS_TILEDWINDOW | WS_CLIPCHILDREN | WS_MAXIMIZE) & ~WS_THICKFRAME ;
    RECT tWndRc = {0,0,640,480}, tWorkRc;
    AdjustWindowRectEx( &tWndRc , cStyle , TRUE , cStyleEx );
    
    //center it in the workarea
    _const iWid = tWndRc.right-tWndRc.left;
    _const iHei = tWndRc.bottom-tWndRc.top;
    SystemParametersInfo( SPI_GETWORKAREA , 0 , &tWorkRc , false );
    tWndRc.left = tWorkRc.left + (((tWorkRc.right-tWorkRc.left)-iWid)/2);
    tWndRc.top  = tWorkRc.top  + (((tWorkRc.bottom-tWorkRc.top)-iHei)/2);
    tWndRc.right = tWndRc.left + iWid ; tWndRc.bottom = tWndRc.top + iHei;    
    
    hwnd = CreateWindowEx(cStyleEx,g_pzAppName,g_pzAppName,cStyle,tWndRc.left,tWndRc.top,iWid,iHei,NULL,g_WndMenu,g_APPINSTANCE,0);    
    if (fnSetWindowTheme) { fnSetWindowTheme( hwnd , L"" , L"" ); }
    //SetLayeredWindowAttributes( hwnd , 0 , 192 , 0 );

    // Process windows messages
    // *** all messages(events) will be read converted/dispatched here ***
    UpdateWindow( hwnd );
    ShowWindow( hwnd , SW_SHOWMAXIMIZED ); //SW_SHOW
        

    while( GetMessage( &wMsg, NULL, 0, 0 ) ) {
        //if (IsDialogMessage( hWnd ,@wMsg )) { continue; }
        if ( (wMsg.message == WM_MOUSEMOVE) && (wMsg.hwnd != hwnd) ) {
            SendMessage( hwnd , WM_NCMOUSEMOVE , HTCLIENT , 0 );
        }
        TranslateMessage( &wMsg );
        DispatchMessage( &wMsg );  
    } //while

    DestroyWindow( hwnd );
    return 0;
  
}; // main()
