#define _ForEachControl( _Do ) \
    /*      ID    , ExStyle ,    Class   ,      Caption     ,      Style     , xPos , yPos , Width,Height */\
    _Do( wcButton ,    0    , "button"   , "Click"          , cStyle         ,   10 ,   10 ,   80 ,   24 ) \
    _Do( wcEdit   ,  cBrd   , "edit"     , "Hello World "   , cTxtStyle      ,   10 ,   44 ,  320 ,  240 ) \
/*----------------------------------------------------------------------------------------------------------- */

//*************** Enumerating our control id's ***********
#define _EnumCtl( mID , ... ) mID,
typedef enum {
    wcMain,
    _ForEachControl( _EnumCtl )
    wcLast
} WindowControls;
#undef _EnumCtl

HWND g_CTL[wcLast];       //controls

LRESULT wndCreate( HWND hwnd ) {    
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
    _ForEachControl( _AddCtl )    

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
}