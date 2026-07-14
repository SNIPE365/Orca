#define main_module Orca

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <windows.h>
#include <commctrl.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)


//*************** Enumerating our control id's ***********
typedef enum {
    wcMain,
    wcButton,
    wcEdit,
    wcLast
} WindowControls;

HWND g_CTL[wcLast];       //controls
HINSTANCE g_APPINSTANCE;  //instance
HFONT g_MainFont;         //fonts
HMENU g_WndMenu;          //menu
//AppName
char* g_pzAppName = "GUI Example";        

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

#include "modules\menu.c"

// *************** Procedure Function ****************
CALLBACK LRESULT WndProc ( HWND hwnd , UINT message, WPARAM wparam, LPARAM lparam ) {  
    #define _CTL(_ctlId) g_CTL[_ctlId]
    
    switch ( message ) {
        case WM_CREATE:  { //Window was created    
            if (_CTL(wcMain)) { return 0; }
            _CTL(wcMain) = hwnd;
        
            //just a macro to help creating controls
            #define _AddCtl( mID , mExStyle , mClass , mCaption , mStyle , mX , mY , mWid , mHei ) _CTL(mID) = CreateWindowEx(mExStyle,mClass,mCaption,mStyle,mX,mY,mWid,mHei,hwnd,(HMENU)(mID),g_APPINSTANCE,NULL);
            
            _const UpDn = UPDOWN_CLASS;    
            _const cStyle = WS_CHILD | WS_VISIBLE;            //Standard style for buttons class controls :)    
            _const cUpDnStyle = cStyle | UDS_AUTOBUDDY;       //' or UDS_SETBUDDYINT  
            _const cButtonStyle = cStyle;
            _const cLabelStyle = cStyle;    
            _const cTxtStyle = cStyle | ES_AUTOVSCROLL | WS_VSCROLL | ES_MULTILINE;
            _const RichStyle = cStyle | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | ES_MULTILINE;    
            _const cBrd = WS_EX_CLIENTEDGE;
        
            // **** Creating a Control ****
            _AddCtl( wcButton , 0    , "button" , "Click"        , cStyle      , 10 , 10 , 80 , 24   );
            _AddCtl( wcEdit   , cBrd , "edit"   , "Hello World " , cTxtStyle  , 10 , 44 , 320 , 240 );
        
            // **** Creating a font ****
            HDC hDC = GetDC(hwnd); //can be used for other stuff that requires a temporary DC
            int nHeight = -MulDiv(12, GetDeviceCaps(hDC, LOGPIXELSY), 72); //'calculate size matching DPI
        
            g_MainFont = CreateFont(nHeight,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,DRAFT_QUALITY | ANTIALIASED_QUALITY,0,"Verdana");
            // **** Setting this font for all controls ****
            for (int CNT = wcMain ; CNT < wcLast ; CNT++ ) {
                SendMessage( _CTL(CNT) , WM_SETFONT , (WPARAM)g_MainFont , true );
            }
            SendMessage( _CTL(wcEdit) , EM_SETLIMITTEXT , 0 , 0 );
        
            ReleaseDC(hwnd,hDC);    
            SetFocus(hwnd);
        
            return 1;
        } // WM_CREATE
        case WM_COMMAND: { //Event happened to a control (child window/control)    
            switch HIWORD(wparam) {
                case EN_CHANGE:  {
                    printf("%i\n,",SendMessage(_CTL(wcEdit),WM_GETTEXTLENGTH,0,0));
                } //EN_CHANGE
                case BN_CLICKED: { //button click            
                    switch (LOWORD(wparam)) {
                        case wcButton: {
                            MessageBox( hwnd , "Bye" , "Bye" , MB_ICONINFORMATION );
                            PostQuitMessage(0);
                            break;        
                        } //wcButton
                    } // switch (lparam)
                } //BN_CLICKED
            } //switch HIWORD(wparam)
            return 0;
        }
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
    
    #undef _CTL    
} //WndProc()

/* *********************************************************************
   *********************** SETUP MAIN WINDOW ***************************
   ******************* This code can be ignored ************************
   ********************************************************************* */

int main() {

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
    _const cStyleEx = 0; //WS_EX_COMPOSITED or WS_EX_LAYERED
    _const cStyle   = WS_VISIBLE | WS_TILEDWINDOW | WS_CLIPCHILDREN | WS_MAXIMIZE;
    RECT tWndRc = {0,0,640,480};
    AdjustWindowRectEx( &tWndRc , cStyle , TRUE , cStyleEx );
    hwnd = CreateWindowEx(cStyleEx,g_pzAppName,g_pzAppName,cStyle,
        200,200,tWndRc.right-tWndRc.left,tWndRc.bottom-tWndRc.top,NULL,g_WndMenu,g_APPINSTANCE,0);

    // Process windows messages
    // *** all messages(events) will be read converted/dispatched here ***
    UpdateWindow( hwnd );

    while( GetMessage( &wMsg, NULL, 0, 0 ) ) {
        //if (IsDialogMessage( hWnd ,@wMsg )) { continue; }
        TranslateMessage( &wMsg );
        DispatchMessage( &wMsg );  
    } //while

    DestroyWindow( hwnd );
    return 0;
  
}; // main()
