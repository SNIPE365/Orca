#define _StP(_ID,_Off) { .bEnd=0 , .bUnit=0 , .iID = (_ID) , .iOffset = ((_Off)*10) }
#define _EnP(_ID,_Off) { .bEnd=1 , .bUnit=0 , .iID = (_ID) , .iOffset = ((_Off)*10) }
#define _StT(_ID,_Off) { .bEnd=0 , .bUnit=1 , .iID = (_ID) , .iOffset = ((_Off)*100) }
#define _EnT(_ID,_Off) { .bEnd=1 , .bUnit=1 , .iID = (_ID) , .iOffset = ((_Off)*100) }
#define _PrevID iCurID-1
#define _NextID iCurID+1
#define _PrevCtl() _EnP(_PrevID,0)
#define _NextCtl() _StP(_NextID,0)

#define _ForEachControl( _Do ) \
    /*      ID           , ExStyle ,    Class    ,    Style     ,     xPos    ,     yPos             ,     Width   ,   Height             ,     Caption    */\
    _Do( wcPanComponents ,  cBsc   , WC_TREEVIEW , cTreeStyle  , _StP(0, 0)  , _StP(0, 0)           , _StP(0, 20) , _StP(wcEdtConsole,0) , "Components"    ) \
    _Do( wcDgmFilter     ,    0    , "ComboBox"  , cComboStyle  , _StP(0,20)  , _StP(0, 0)           , _StP(0, 20) , _StT(0,1.25)         , NULL            ) \
    _Do( wcDgmSelect     ,    0    , "ComboBox"  , cComboStyle  , _PrevCtl()  , _StP(0, 0)           , _NextCtl()  , _StT(0,1.25)         , NULL            ) \
    _Do( wcPanProperties ,  cBsc   , "listbox"   , cPanelStyle  , _StP(0,80)  , _StP(0, 0)           , _StP(0, 20) , _StP(wcEdtConsole,0) , "Properties"    ) \
    _Do( wcDiagram       ,    0    , "Diagram"   , cCodeStyle   , _StP(0,20)  , _EnT(wcDgmSelect,.25), _StP(0, 60) , _StP(wcEdtConsole,0) , NULL            ) \
    _Do( wcEdtConsole    ,  cBrd   , "edit"      , cTxtStyle    , _StP(0, 0)  , _StP(0,75)           , _StP(0,100) , _EnT(0,-1.25)        , NULL            ) \
    _Do( wcBtnBuild      ,    0    , "button"    , cStyle       , _StP(0, 0)  , _EnP(wcEdtConsole,0) , _StT(0,8.0) , _StT(0,1.25)         , "Build"         ) \
    _Do( wcEdtCmd        ,  cBrd   , "edit"      , cEdtStyle    , _PrevCtl()  , _EnP(wcEdtConsole,0) , _StP(0, 95) , _StT(0,1.25)         , ""              )
/*----------------------------------------------------------------------------------------------------------- */

typedef struct {
  HFONT hFont;
  int   iHeight;
  int   iPixW,iPixH;
} FontStruct;
typedef struct {
    uint32_t bEnd    :1  ; //0=relative to start , 1=relative to end
    uint32_t bUnit   :2  ; //0=percent , 1=twips , 2=???? , 3=????
    uint32_t iID     :12 ; //ID of the relative control
    int32_t  iOffset :17 ; //Offset of the relative position with the bUnit type
} CtlPos;
typedef struct {
    HWND        hwnd;
    FontStruct* pFont;
    CtlPos tX,tY,tW,tH;    //position,size in relative percent/twips
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
#define _EnumCtlName( mID , ... ) #mID,
char* g_pzCtl[] = {
    "Main",
    _ForEachControl( _EnumCtlName )
    "Last"
};

WindowStruct g_tMain;
FontStruct g_tMainFont;

ControlStruct g_CTL[wcLast];       //controls
#define _CTL(_ctlId) g_CTL[_ctlId].hwnd

//percent 10000=100% , twip 100=100%
#define _Pct2X( _pct ) (((_pct)*(g_tMain.iW))/1000)
#define _Pct2Y( _pct ) (((_pct)*(g_tMain.iH))/1000)
#define _X2Pct( _X ) (((_X)*/1000)/(g_tMain.iW))
#define _Y2Pct( _Y ) (((_Y)*/1000)/(g_tMain.iH))
#define _Twp2X( _twp ) (((_twp)*(w->pFont->iPixW))/100)
#define _Twp2Y( _twp ) (((_twp)*(w->pFont->iPixH))/100)

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
    g_CTL[wcMain].iPW = g_tMain.iW = tRc.right;
    g_CTL[wcMain].iPH = g_tMain.iH = tRc.bottom;

    //resize controls
    HDWP pDefer = BeginDeferWindowPos( (wcLast-(wcMain))-1 );
    _const cNotProcessed = -(1<<30);
    for ( int i = wcMain+1 ; i < wcLast ; i++ ) { g_CTL[i].iPH = cNotProcessed; }
    int iProcessed = 1;
    for ( ; iProcessed ; ) {
        iProcessed = 0;
        for (int i = wcMain+1 ; i < wcLast ; i++ ) {
            _with( g_CTL[i] ) {
                //skip if already processed or if dependency not processed yet
                if (w->iPH != cNotProcessed) { continue; }
                if (g_CTL[w->tX.iID].iPH == cNotProcessed) { continue; }
                if (g_CTL[w->tY.iID].iPH == cNotProcessed) { continue; }
                if (g_CTL[w->tW.iID].iPH == cNotProcessed) { continue; }
                if (g_CTL[w->tH.iID].iPH == cNotProcessed) { continue; }

                iProcessed++; //at least one control processed in this iteration (so there will be another iteration)

                //Calculate X Position
                w->iPX = g_CTL[w->tX.iID].iPX + ((w->tX.bUnit > 0) ? (_Twp2X(w->tX.iOffset)) : (_Pct2X(w->tX.iOffset))); //relative start + unit scaled offset
                if ( w->tX.bEnd ) { w->iPX += g_CTL[w->tX.iID].iPW; } //if relative to END add width
                //Calculate Y Position
                w->iPY = g_CTL[w->tY.iID].iPY + ((w->tY.bUnit > 0) ? (_Twp2Y(w->tY.iOffset)) : (_Pct2Y(w->tY.iOffset))); //relative start + unit scaled offset
                if ( w->tY.bEnd ) { w->iPY += g_CTL[w->tY.iID].iPH; } //if relative to END add height
                //Calculate Width
                w->iPW = (w->tW.bUnit > 0) ? (_Twp2X(w->tW.iOffset)) : (_Pct2X(w->tW.iOffset)); //absolute unit scale width
                if ( w->tW.iID || w->tW.bEnd ) { //if width is NOT relative to window start then it stretched to that point
                    int iRef = g_CTL[w->tW.iID].iPX + ( w->tW.bEnd ? g_CTL[w->tW.iID].iPW : 0 );
                    w->iPW += iRef-w->iPX;
                }
                //Calculate Height
                w->iPH = (w->tH.bUnit > 0) ? (_Twp2Y(w->tH.iOffset)) : (_Pct2Y(w->tH.iOffset)); //absolute unit scale height
                if ( w->tH.iID || w->tH.bEnd ) { //if height is NOT relative to window start then it streches to that point
                    int iRef = g_CTL[w->tH.iID].iPY + ( w->tH.bEnd ? g_CTL[w->tH.iID].iPH : 0 );
                    w->iPH += iRef-w->iPY;
                }

                //printf("%-20s  x=%4i ,  y=%4i ,  w=%4i ,  h=%4i\n" ,  g_pzCtl[i], w->tX.iOffset , w->tY.iOffset , w->tW.iOffset , w->tH.iOffset );
                //printf("%-20s px=%4i , py=%4i , pw=%4i , ph=%4i\n" ,  g_pzCtl[i], w->iPX , w->iPY , w->iPW , w->iPH );
                DeferWindowPos( pDefer , w->hwnd , NULL , w->iPX,w->iPY, w->iPW,w->iPH , SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW );
            } _endwith
        }
    }
    EndDeferWindowPos( pDefer );
}

LRESULT wndCreate( HWND hwnd ) {
    #include "controlMacros.c"

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

    emSetLimitText( wcEdtCmd , 0 );
    //Initialize component palette
    TV_INSERTSTRUCT tItem = { .hParent = TVI_ROOT , .hInsertAfter = TVI_SORT , .item.mask = TVIF_TEXT | TVIF_PARAM };
        for (int i=1 ; i < _countof(g_atClassGroup) ; i++ ) {
        _with( g_atClassGroup[i] ) {
            tItem.item.pszText = w->pzName;
            w->hITEM = TreeView_InsertItem( _CTL(wcPanComponents) , &tItem );
        } _endwith
    }
    for (int i=1 ; i < _countof(g_ClassInterface) ; i++ ) {
        _with( g_ClassInterface[i] ) {
            tItem.hParent = g_atClassGroup[ w->bGroup ].hITEM;
            tItem.item.pszText = w->pzName;
            TreeView_InsertItem( _CTL(wcPanComponents) , &tItem );
        } _endwith
    }


    cbAddString( wcDgmSelect , "Global" , 0 );
    cbAddString( wcDgmSelect , "Devices (I/O)" , 0 );
    cbAddString( wcDgmSelect , "Components" , 0 );
    iResu = cbAddString( wcDgmSelect , "Main" , 0 );
    SetFocus( _CTL(wcDgmSelect) );
    cbSetCurSel( wcDgmSelect , iResu );
    SetFocus( _CTL(wcEdtCmd) );

    puts("ready!");


    SetFocus(hwnd);

    return 1;
}
