#define _ForEachControl( _Do ) \
    /*      ID           , ExStyle ,    Class   ,      Caption     ,      Style     , xPos , yPos , Width,Height */\
    _Do( wcPanComponents ,  cBsc   , "listbox"   , "Components"     , cPanelStyle    ,    0 ,    0 ,  2000, 7500 ) \
    _Do( wcPanCode       ,  cBsc   , "listbox"   , "Code"           , cCodeStyle     , 2000 ,    0 ,  6000, 7500 ) \
    _Do( wcPanProperties ,  cBsc   , "listbox"   , "Properties"     , cPanelStyle    , 8000 ,    0 ,  2000, 7500 ) \
    _Do( wcEdtConsole    ,  cBrd   , "edit"     , "Console..."     , cTxtStyle      ,    0 , 7500 , 10000 , 2000 ) \
    _Do( wcBtnCmd        ,    0    , "button"   , "Cmd ->"         , cStyle         ,    0 , 9500 ,  -800 , -125 ) \
    _Do( wcEdtCmd        ,  cBrd   , "edit"     , ""               , cEdtStyle      , -800 , 9500 ,  95000, -125 )
/*----------------------------------------------------------------------------------------------------------- */

typedef struct {
  HFONT hFont;
  int   iHeight;
  int   iPixW,iPixH;
} FontStruct;

typedef struct {
    HWND        hwnd;
    FontStruct* pFont;
    int iX,iY,iW,iH;       //position,size in percent/twips    
    int iPX,iPY,iPW,iPH;   //position,size in pixels
    int iFntW,iFntH;
} ControlStruct;

//todo effectively use this for windows
typedef struct {
    HWND hwnd;
    int iX,iY,iW,iH;       //position,size     
    int iCtlCnt;
    ControlStruct* pCTL;
} WindowStruct;

//*************** Enumerating our control id's ***********
#define _EnumCtl( mID , ... ) mID,
typedef enum {
    wcMain,
    _ForEachControl( _EnumCtl )
    wcLast
} WindowControls;
#undef _EnumCtl

WindowStruct g_tMain;
FontStruct g_tMainFont;
 
ControlStruct g_CTL[wcLast];       //controls
#define _CTL(_ctlId) g_CTL[_ctlId].hwnd

//percent 10000=100% , twip 100=100%
#define _Pct2X( _pct ) (((_pct)*(g_tMain.iW))/10000)
#define _Pct2Y( _pct ) (((_pct)*(g_tMain.iH))/10000)
#define _X2Pct( _X ) (((_X)*/10000)/(g_tMain.iW))
#define _Y2Pct( _Y ) (((_Y)*/10000)/(g_tMain.iH))
#define _Twp2X( _twp ) (((_twp)*(w->pFont->iPixW))/-100)
#define _Twp2Y( _twp ) (((_twp)*(w->pFont->iPixH))/-100)

FontStruct* FontCreate( FontStruct* pFont , char* pzFace , int iHeight , int iBold ) {
    HDC hDC = GetDC(0); //can be used for other stuff that requires a temporary DC
    _with( *pFont ) {
        int nHeight = -MulDiv( iHeight , GetDeviceCaps(hDC, LOGPIXELSY), 72); //'calculate size matching DPI
        w->iHeight = iHeight;
        w->hFont   = CreateFont(nHeight,0,0,0,iBold?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,DRAFT_QUALITY | ANTIALIASED_QUALITY,0,pzFace);
        HFONT hOld = SelectObject( hDC , w->hFont ); SIZE tSz;
        GetTextExtentPoint32( hDC , "|W^_´" , 5 , &tSz );
        w->iPixW = tSz.cx/5;
        w->iPixH = tSz.cy;
        printf("Font w=%i h=%i\n",w->iPixW,w->iPixH);
        SelectObject( hDC , hOld );
        ReleaseDC( 0 , hDC );
    } _endwith
    return pFont;
}    

void wndResize( HWND hwnd ) {
    //grab current window size and set it to the structure
    RECT tRc ; GetClientRect( hwnd , &tRc );
    g_tMain.iW = tRc.right; 
    g_tMain.iH = tRc.bottom;
    
    //resize controls
    HDWP pDefer = BeginDeferWindowPos( (wcLast-(wcMain))-1 );    
    for (int i = wcMain+1 ; i < wcLast ; i++ ) {
        _with( g_CTL[i] ) {
            w->iPX = (w->iX < 0) ? (_Twp2X(w->iX)) : (_Pct2X(w->iX)); //
            w->iPY = (w->iY < 0) ? (_Twp2Y(w->iY)) : (_Pct2Y(w->iY)); // > decide if percent (positive)
            w->iPW = (w->iW < 0) ? (_Twp2X(w->iW)) : (_Pct2X(w->iW)); //   or twips (negative) are used.
            w->iPH = (w->iH < 0) ? (_Twp2Y(w->iH)) : (_Pct2Y(w->iH)); //
            printf("%i , x=%i(%i) , y=%i(%i) , w=%i(%i) , h=%i(%i)\n" , 
                i, w->iX,w->iPX , w->iY,w->iPY , w->iW,w->iPW , w->iH,w->iPH );
            DeferWindowPos( pDefer , w->hwnd , NULL , w->iPX,w->iPY, w->iPW,w->iPH , SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW );
        } _endwith
    }    
    EndDeferWindowPos( pDefer );
}    

LRESULT wndCreate( HWND hwnd ) {    
    if (_CTL(wcMain)) { return 0; }
    _CTL(wcMain) = hwnd;

    //just a macro to help creating controls
    #define _AddCtl( mID , mExStyle , mClass , mCaption , mStyle , mX , mY , mWid , mHei ) g_CTL[mID] = (ControlStruct){ .iX = mX , .iY = mY , .iW = mWid , .iH = mHei , .hwnd = CreateWindowEx(mExStyle,mClass,mCaption,mStyle,0,0,1,1,hwnd,(HMENU)(mID),g_APPINSTANCE,NULL) };

    _const UpDn = UPDOWN_CLASS;    
    _const cStyle = WS_CHILD;            //Standard style for buttons class controls :)    
    _const cUpDnStyle = cStyle | UDS_AUTOBUDDY;       //' or UDS_SETBUDDYINT  
    _const cButtonStyle = cStyle;
    _const cLabelStyle = cStyle;    
    _const cEdtStyle = cStyle | ES_AUTOHSCROLL;     
    _const cTxtStyle = cStyle | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | ES_MULTILINE;    
    _const cPanelStyle = cStyle | WS_VSCROLL | LBS_NOINTEGRALHEIGHT;
    _const cCodeStyle = cPanelStyle | WS_HSCROLL;
    
    _const cBrd = WS_EX_CLIENTEDGE;
    _const cBsc = WS_EX_DLGMODALFRAME;


    // **** Creating a Control ****
    _ForEachControl( _AddCtl )    

    // **** Creating a font ****    
    _auto pMainFnt = FontCreate( &g_tMainFont , "verdana" , 12 , false );
    
    puts("setting fonts");    
    // **** Setting this font for all controls ****    
    for (int i = wcMain ; i < wcLast ; i++ ) {
        _with( g_CTL[i] ) {
            if (w->hwnd) {
                w->pFont = pMainFnt;            
                SendMessage( w->hwnd , WM_SETFONT , (WPARAM)pMainFnt->hFont , false );            
            }        
        } _endwith
    }
    puts("ready!");
    SendMessage( _CTL(wcEdtCmd) , EM_SETLIMITTEXT , 0 , 0 );
    SetFocus(hwnd);

    return 1;
}