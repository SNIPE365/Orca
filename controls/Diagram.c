typedef struct {
    int32_t iY;
    uint32_t iIndex;
    int16_t iX;
    uint8_t iW,iH;
    char zCaption[15];
    uint8_t bColor;
} DiagramObjectStruct;

static CALLBACK LRESULT Diagram_WndProc ( HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam ) {
    
    typedef enum {
        DIM_BASE = WM_USER,
        DIM_INSERT,
        DIM_REMOVE,
        /* PRIVATE ONES */
        DIM_CREATE_BUFFER,    
    } DiagramEnum;
    typedef enum {
        dmtRedraw = 1,
    } DiagramTimers;
    
    static HBITMAP hBmBuffer;
    static HFONT hCtlFont;
    static HDC hDcBuffer;
    static int iBufWid,iBufHei;
    static char bDrawn=1,bUpdateScroll=0,bHScroll=0,bVScroll=0;       
    _const cBack=0xFFFFFF ; _const cGrid=0xEEEEEE ; _const cSelected=0x101010;
    static const int32_t cObject[] = { 0xFF8844 , 0xFF4488 , 0x44FF88 , 0x4488FF , 0x8844FF , 0x88FF44 , -1 };
    static HBRUSH hbBack , hbBackGrid , hbObject[256] = {0} ;
    static HPEN hpSelected;
    
    #define SetUpdateAsync() if (bDrawn) { bDrawn=0 ; SetTimer( hwnd , dmtRedraw , 7 , NULL ); }
    #define SetUpdate() if (bDrawn) { bDrawn=0 ; SendMessage( hwnd , WM_TIMER , 0 , 0 ); SetTimer( hwnd , dmtRedraw , 1000/120 , NULL ); }
    #define aObject(_I) ptObjects[piOrder[_I]]
    _const _MaxGap = 128;
    
    static int iObjCount=0, iObjMaxCount=_MaxGap; //object counting / limit
    static int iMaxX=0, iMaxY=0;                  //maximum position of any existing object    
    static int iViewX=0, iViewY=0;                //scrolling offset
    static int iMaxXIdx=-1 , iMaxYIdx=-1;         //indexes for the objects that have the maximum position (cache)
    static int iStartIdx=0, iEndIdx=-1;           //start/end indexes for drawn objects (cache)
    static int iSelectedIndex=-1;                 //current selected index
    static int iMouseX=0,iMouseY=0;               //last mouse position
    static int iDragStartX,iDragStartY;           //position where drag started (if dragging)
    static int iDragCancelX,iDragCancelY;         //original position of dragged element (if dragging)
    static char bDragging=0;                      //0=no drag, 1=drag may start, 2=dragging
    
    static DiagramObjectStruct* ptObjects = NULL;
    static uint32_t* piOrder = NULL;
            
    // ------------- Diagram functions -------------
    void ScrollUpdate( HWND hwnd , int nWid , int nHei ) {
        //if client are is not given... calculate
        if (nWid < 0) {
            RECT tRc ; GetClientRect( hwnd , &tRc );
            nWid = tRc.right ; nHei = tRc.bottom;
        }
        //if max size/indexes need to be found, do it now
        if (iMaxXIdx < 0) {
            iMaxX = iMaxY = -1;
            for (int i=0 ; i < iObjCount ; i++ ) {
                _with( aObject(i) ) {                    
                    if ((w->iX+w->iW) > iMaxX) { iMaxX = (w->iX+w->iW); iMaxXIdx=i; }
                    if ((w->iY+w->iH) > iMaxY) { iMaxY = (w->iY+w->iH); iMaxYIdx=i; }
                } _endwith;
            }
        }
        
        //update scrollbar max sizes , page , range
        SCROLLINFO tInfoH = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL , .nPage = nWid , .nMin = 0 , .nMax = (iMaxX >= nWid) ? iMaxX+nWid/2 : iMaxX };
        SetScrollInfo( hwnd , SB_HORZ , &tInfoH , true );            
        SCROLLINFO tInfoV = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL , .nPage = nHei , .nMin = 0 , .nMax = (iMaxY >= nHei) ? iMaxY+nHei/2 : iMaxY };
        SetScrollInfo( hwnd , SB_VERT , &tInfoV , true );
        bUpdateScroll = 1;
    }
    void WindowDraw(void) {
        
        //check if there's previous items that are visible (caused by moving or scroll up)
        if (bDrawn==1) { return; }                                         
        
        HDC hdc = hDcBuffer;
        RECT tRc = {0,0,iBufWid,iBufHei};
                    
        iViewX = GetScrollPos( hwnd , SB_HORZ );            
        int iTempViewY = GetScrollPos( hwnd , SB_VERT );
        iViewY = (iViewY*3+iTempViewY+2)/4;
        if ((abs(iViewY-iTempViewY) > iBufHei)) { iViewY = (iViewY+iTempViewY+1)/2; }
        if (iTempViewY < iViewY) { iViewY--; }
        if (iTempViewY > iViewY) { iViewY++; }
        
        SetBrushOrgEx( hdc , 1 , 4-(iViewY & 7) , NULL );
        SetBrushOrgEx( hdc , 4 , 4-(iViewY & 7) , NULL );
        FillRect( hdc , &tRc , hbBackGrid );
        SetBkMode( hdc , TRANSPARENT );        
        
        if (!piOrder) { return; }
        //puts("drawing start");
        
        for ( ; iStartIdx>0 ; iStartIdx-- ) {
            _with( aObject(iStartIdx-1) ) {
                if ((w->iY+w->iH-iViewY) < 0) { break; }
            } _endwith;
        }
        
        int iIndex;
        for ( iIndex=iStartIdx ; (iIndex < iObjCount) ; iIndex++) {
            _with( aObject(iIndex) ) {
                //check/skip if item is invisible (caused by moving or scroll down)
                if ((w->iY+w->iH-iViewY) < 0) { iStartIdx += 1 ; continue ; }                    
                //clculate position Y and see if it's after the visible area (early stop)
                int iPosY = w->iY-iViewY, iPosX = w->iX-iViewX;
                if (iPosY >= iBufHei) { break; }
                //skip object if outside horizontal range
                if ( (iPosX+w->iW) < 0 || iPosX >= iBufWid ) { continue; }                    
                //render object
                SelectObject( hdc , hbObject[ w->bColor ] );
                RECT tObjRc = {iPosX,iPosY,iPosX+w->iW,iPosY+w->iH};
                RoundRect( hdc , tObjRc.left , tObjRc.top , tObjRc.right , tObjRc.bottom , 16 , 16 );                
                if (w->zCaption) {
                    DrawText( hdc , w->zCaption , -1 , &tObjRc , DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
                }
                
                if (iIndex == iSelectedIndex) { 
                    _const hOldPen = SelectObject( hdc , hpSelected );
                    _const hOldBrush = SelectObject( hdc , GetStockObject( NULL_BRUSH ) ); 
                    RoundRect( hdc , tObjRc.left-0 , tObjRc.top-0 , tObjRc.right+0 , tObjRc.bottom+0 , 16 , 16 );
                    SelectObject( hdc , hOldPen ); SelectObject( hdc , hOldBrush );
                }                        
                
                if (iIndex < (iObjCount-1)) {
                    iPosX += w->iW/2; iPosY += w->iH;
                    MoveToEx( hdc , iPosX , iPosY , NULL ); LineTo( hdc , iPosX , iPosY+24 );
                }
                
                
                
            } _endwith;
        }
        
        iEndIdx=(iIndex < iObjCount ? iIndex-1 : iObjCount-1); 
        //puts("drawing end");
        
        bDrawn = 2;
        if (iViewY == iTempViewY) { bDrawn = 1; if (wParam) { KillTimer(hwnd,wParam); } }
        InvalidateRect( hwnd , NULL , true ); //UpdateWindow( hwnd );
        return;
    }; //void DrawWindow(void)
    int InsertObject( int iPosX , int iPosY ) {
        
        //increase storage if needed
        if (iObjCount >= iObjMaxCount) {
            iObjMaxCount += _MaxGap;
            piOrder = realloc( piOrder , iObjMaxCount*sizeof(uint32_t) );
            ptObjects = realloc( ptObjects , iObjMaxCount*sizeof(DiagramObjectStruct) );
            //todo: check for faillure
        }
        int iNew;
        for (iNew = iObjCount ; iNew > 0 ; iNew--) {
            _with( aObject(iNew-1) ) {
              if ((w->iY) < iPosY) { break; }
              piOrder[iNew] = piOrder[iNew-1];
            } _endwith;
        }   
        piOrder[iNew] = iObjCount;
        _with( ptObjects[iObjCount] ) {
            w->iIndex = iNew;
            w->iX = iPosX;
            w->iW = (48+(rand() % 120)) & (~7);
            w->iY = iPosY; 
            w->iH = (32+(rand() % 64)) & (~7);
            w->bColor = rand() % 6;
            sprintf(w->zCaption , "Obj%i", iObjCount+1 );
        } _endwith;
        iObjCount++;
        SetUpdate();        
    }    
    
    // ------------- Message dispatch --------------
    switch (message) {
        case WM_ERASEBKGND: { return 1; }
        case WM_SETCURSOR: {
            
            //if dragging show moving cursor
            if (bDragging==2) {
                SetCursor( LoadCursor( NULL , IDC_SIZEALL ) );
                return 0;
            }
            //if not check if hovering over a visible object            
            const POINT pt = { iMouseX , iMouseY };
            static int iLastIndex = -1;
            if (iLastIndex > iStartIdx) { 
                _with( aObject(iLastIndex) ) {
                    const RECT tRc = { .left = w->iX-iViewX , .top = w->iY-iViewY , .right = w->iX-iViewX+w->iW , .bottom = w->iY-iViewY+w->iH };                                        
                    if (PtInRect( &tRc , pt )) { SetCursor( LoadCursor( NULL , IDC_HAND ) ) ; return 0; }
                } _endwith;
            }
            for ( int iIndex = iStartIdx ; iIndex<=iEndIdx ; iIndex++ ) {                
                _with( aObject(iIndex) ) {
                    const RECT tRc = { .left = w->iX-iViewX , .top = w->iY-iViewY , .right = w->iX-iViewX+w->iW , .bottom = w->iY-iViewY+w->iH };                                        
                    if (PtInRect( &tRc , pt )) { SetCursor( LoadCursor( NULL , IDC_HAND ) ) ; iLastIndex=iIndex ; return 0; }
                } _endwith;
            }       
            //otherwise DefWindowProc will set default cursor
            break;
        }
        case WM_MOUSEMOVE: {       //Mouse moved in the control
            iMouseX = (short)LOWORD(lParam);  // horizontal position of cursor 
            iMouseY = (short)HIWORD(lParam);  // vertical position of cursor
            //check if moved enough to start a drag, to active it and backup initial position
            if ((bDragging==1) && ((abs(iMouseX-iDragStartX)>3) || (abs(iMouseY-iDragStartY)>3))) {
                iDragCancelX = aObject(iSelectedIndex).iX; iDragCancelY = aObject(iSelectedIndex).iY; bDragging = 2; 
            }
            //if dragging move the block aligned to the grid;
            if (bDragging==2) {
                _with( aObject(iSelectedIndex) ) {
                    const int iNewX = ((iDragCancelX+(iMouseX-iDragStartX))+3) & ~7;
                    const int iNewY = ((iDragCancelY+(iMouseY-iDragStartY))+3) & ~7;
                    if ((iNewX != w->iX) || (iNewY != w->iY)) {
                        w->iX = iNewX; w->iY= iNewY;
                        //reorder the dragging object
                        while (1) {
                            if (iSelectedIndex < (iObjCount-1)) {
                                const int iNextY = aObject(iSelectedIndex+1).iY;
                                if ( (iNewY > iNextY) || ((iNewY==iNextY) && (iNewX > aObject(iSelectedIndex+1).iX)) ) {
                                    SWAP( piOrder[iSelectedIndex] , piOrder[iSelectedIndex+1] ); iSelectedIndex++; continue;
                                } //endif
                            } //endif
                            if (iSelectedIndex > 0) {
                                const int iPrevY = aObject(iSelectedIndex-1).iY;
                                if ( (iNewY < iPrevY) || ((iNewY==iPrevY) && (iNewX < aObject(iSelectedIndex-1).iX)) ) {
                                    SWAP( piOrder[iSelectedIndex] , piOrder[iSelectedIndex-1] ); iSelectedIndex--; continue;
                                } //endif
                            } //endif (iSelectedIndex > 0) {
                            break;
                        } //wend
                        
                        SetUpdate();
                    } //endif
                } _endwith;                
                _with( aObject(iSelectedIndex) ) {
                    bool bUpdate=0;
                    if ((w->iX+w->iW) > iMaxX) { iMaxX = (w->iX+w->iW); iMaxXIdx=iSelectedIndex; bUpdate=true; }
                    if ((w->iY+w->iH) > iMaxY) { iMaxY = (w->iY+w->iH); iMaxYIdx=iSelectedIndex; bUpdate=true; }
                    if (bUpdate) { ScrollUpdate(hwnd,-1,-1); }
                } _endwith;
                // if mouse is outside of visible area then scroll it
                static char bSwap ; bSwap = (bSwap+1) & 3;
                if (!bSwap) {
                    RECT tRc; GetClientRect( hwnd , &tRc );                
                    if (iMouseY < 0)           { PostMessage( hwnd , WM_VSCROLL , SB_LINEUP   , 0 ); }
                    if (iMouseY >= tRc.bottom) { PostMessage( hwnd , WM_VSCROLL , SB_LINEDOWN , 0 ); }
                    if (iMouseX < 0)           { PostMessage( hwnd , WM_HSCROLL , SB_LINEUP   , 0 ); }
                    if (iMouseX >= tRc.right)  { PostMessage( hwnd , WM_HSCROLL , SB_LINEDOWN , 0 ); }
                } //endif (bSwap) {
            } //endif (bDragging==2) {
                        
            return 0;
        }
        case WM_PAINT: {           //Update window from bitmap if ready
            if (bDrawn <= 0) { ValidateRect( hwnd , NULL ); return 0; }            
            HDC hdc = (HDC)wParam; // the device context to draw in            
            
            PAINTSTRUCT tPaint;
            if (!wParam) { 
                BeginPaint( hwnd , &tPaint ); hdc = tPaint.hdc;
            } else {
                GetClientRect( hwnd , &tPaint.rcPaint );
            }
            
            _with(tPaint.rcPaint) {
                BitBlt( hdc , w->left , w->top , w->right-w->left , w->bottom-w->top , hDcBuffer , w->left , w->top , SRCCOPY );
            } _endwith
            
            if (!wParam) { EndPaint( hwnd , &tPaint ); }
            return 0;
        }
        case WM_SIZE: {            //Window Size changed discard bitmap
            if (wParam == SIZE_MINIMIZED) { break; }  // resizing flag            
            int nWid = LOWORD(lParam);  // width of client area
            int nHei = HIWORD(lParam); // height of client area
            if ( (nWid > iBufWid) || (nWid <= (iBufWid-64)) || (nHei > iBufHei) || (nHei <= (iBufHei-64)) ) {
                SendMessage( hwnd , DIM_CREATE_BUFFER , 0 , lParam );
                SetUpdate();
            }            
            return 0;
        }        
        case WM_HSCROLL:
        case WM_VSCROLL: {
            _const SB_ = (message==WM_VSCROLL ? SB_VERT : SB_HORZ);
            int nScrollCode = (int)LOWORD(wParam); // scroll bar value
            SCROLLINFO tInfo = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_ALL };
            if (!GetScrollInfo( hwnd , SB_ , &tInfo )) {
                puts("Failed to get diagram scroll info");
            }
            int bFast=1, nPos = tInfo.nPos;
            switch (nScrollCode) {
                case SB_TOP:           { tInfo.nPos = tInfo.nMin; break; }
                case SB_BOTTOM:        { tInfo.nPos = tInfo.nMax; break; }
                case SB_ENDSCROLL:     { break; }
                case SB_LINEDOWN:      { tInfo.nPos = min( nPos+32 , tInfo.nMax ); break; }
                case SB_LINEUP:        { tInfo.nPos = max( nPos-32 , tInfo.nMin ); break; }
                case SB_PAGEDOWN:      { tInfo.nPos = min( nPos+tInfo.nPage , tInfo.nMax ); break; }
                case SB_PAGEUP:        { tInfo.nPos = max( nPos-tInfo.nPage , tInfo.nMin ); break; }
                case SB_THUMBPOSITION: { break; }
                case SB_THUMBTRACK:    { bFast=0; tInfo.nPos = tInfo.nTrackPos; break; }                
            }
            
            tInfo.fMask = SIF_POS;
            if (!SetScrollInfo( hwnd , SB_ , &tInfo , true ) && tInfo.nPos) {
                printf("Failed to set diagram scroll info: %i->%i\n",nPos,tInfo.nPos);
            }            
            //if (tInfo.nPos==tInfo.nMin)             { EnableScrollBar( hwnd , SB_ , ESB_DISABLE_LTUP ); }
            //if (tInfo.nPos>=tInfo.nMax-tInfo.nPage) { EnableScrollBar( hwnd , SB_ , ESB_DISABLE_RTDN ); }            
            if (nPos != tInfo.nPos) { 
                if (bDragging) {
                    GetScrollInfo( hwnd , SB_ , &tInfo );
                    if (message==WM_VSCROLL) { iDragStartY += (nPos-tInfo.nPos); } else { iDragStartX += (nPos-tInfo.nPos); }
                }
                if (nPos == tInfo.nMin)          { EnableScrollBar( hwnd , SB_ , ESB_ENABLE_BOTH ); }
                if (nPos>=tInfo.nMax-tInfo.nPage){ EnableScrollBar( hwnd , SB_ , ESB_ENABLE_BOTH ); }
                if (bFast) { SetUpdate(); } else { SetUpdateAsync(); }
            }            
            return 0;
        }        
        case WM_TIMER: {           //TIMER events (REDRAW!)            
            WindowDraw();
            return 0;
        }
        case DIM_CREATE_BUFFER: {  //INTERNAL: recreate bitmap buffer
            int nWid = LOWORD(lParam);  // width of client area     
            int nHei = HIWORD(lParam); // height of client area            
            iBufWid = 64+((nWid | 63) & (~63)); //gives a room of -63 to 63 extra pixels
            iBufHei = 64+((nHei | 63) & (~63));
            HDC hdc = GetDC( hwnd );
            if (!hDcBuffer) { 
                hDcBuffer = CreateCompatibleDC( hdc ); 
                SelectObject( hdc , hCtlFont );
            }
            hBmBuffer = CreateCompatibleBitmap( hdc , iBufWid , iBufHei );
            DeleteObject( SelectObject( hDcBuffer , hBmBuffer ) );
            ScrollUpdate( hwnd , nWid , nHei );            
            SetUpdate();
            return 1;
        }
        case WM_MOUSEWHEEL: {      //Mouse wheel event
            int zDelta = (short) HIWORD(wParam);    // wheel rotation
            //fwKeys = LOWORD(wParam);    // key flags            
            //xPos = (short) LOWORD(lParam);    // horizontal position of pointer
            //yPos = (short) HIWORD(lParam);    // vertical position of pointer            
            SCROLLINFO tInfo = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_ALL };
            GetScrollInfo( hwnd , SB_VERT , &tInfo );
            const int nPos = tInfo.nPos;
            tInfo.nPos = min( tInfo.nPos+zDelta/-2 , tInfo.nMax );
            tInfo.fMask = SIF_POS;
            SetScrollInfo( hwnd , SB_VERT , &tInfo , true );            
            if (bDragging) { 
                GetScrollInfo( hwnd , SB_VERT , &tInfo );
                iDragStartY += (nPos-tInfo.nPos); 
                SendMessage( hwnd , WM_MOUSEMOVE , 0 , MAKELPARAM( iMouseX , iMouseY ));
            }
            SetUpdate();
            return 0;
        }
        case WM_CREATE: {          //Initialize control
            
            hbBack = CreateSolidBrush( cBack );            
            hbBackGrid = CreateHatchBrush( HS_CROSS , cGrid );
            hpSelected = CreatePen( PS_SOLID , 4 , cSelected );
            PostMessage( hwnd , WM_HSCROLL , 0,0 );
            PostMessage( hwnd , WM_VSCROLL , 0,0 );

            for (int N=0 ; N < 256 ; N++) { //initialize brushes for object palette theme
                if (cObject[N] < 0) { break; }
                hbObject[N] = CreateSolidBrush( cObject[N] );
            }
            
            ptObjects = malloc(iObjMaxCount*sizeof(DiagramObjectStruct));
            piOrder = malloc(iObjMaxCount*sizeof(uint32_t));
            
            // generate semi-random objects for initial tests
            int iPosY=0, iPosX=0, iBigRow ; iObjCount = 32;
            for (int N=0 ; N < iObjCount ; N++ ) {
                piOrder[N] = N;
                if (!iPosX) { iPosX = (rand() % 256) & (~7); iBigRow = 0; }
                _with( ptObjects[N] ) {
                    w->iIndex = N;
                    w->iX = iPosX;
                    w->iW = (48+(rand() % 120)) & (~7);
                    w->iY = iPosY; 
                    w->iH = (32+(rand() % 64)) & (~7);
                    w->bColor = rand() % 6;
                    sprintf(w->zCaption , "Obj%i", N+1 );                    
                    if (w->iH > iBigRow) { iBigRow = w->iH; }                    
                    if ((rand() & 1) || iPosX > 256) { iPosY += (iBigRow+8); iPosX = 0; } else { iPosX += (w->iW+8); }
                    
                } _endwith;
            }
            
            
            return 1;
        }
        case WM_SETFONT: {         //Set New Font
            hCtlFont = (HFONT)wParam; SetUpdate();
            SelectObject( hDcBuffer , hCtlFont );
        }
        case WM_GETFONT: {         //Retrieve Current Font
            return (LRESULT)hCtlFont;
        }
        case WM_KEYDOWN: {         //Key pressed
            switch (wParam) {
                case VK_INSERT: {
                    InsertObject( iViewX+iMouseX , iViewY+iMouseY );
                    break;
                }
            } //switch
            return 0;
        }
        case WM_LBUTTONDOWN: {     //Button pressed
            int iOldSel = iSelectedIndex ; iSelectedIndex = -1;
            //printf("%i to %i\n",iStartIdx,iEndIdx);
            for ( int iIndex = iStartIdx ; iIndex<=iEndIdx ; iIndex++ ) {
                _with( aObject(iIndex) ) {
                    const RECT tRc = { .left = w->iX-iViewX , .top = w->iY-iViewY , .right = w->iX-iViewX+w->iW , .bottom = w->iY-iViewY+w->iH };                    
                    const POINT pt = { (short)LOWORD(lParam) , (short)HIWORD(lParam) };                    
                    if (PtInRect( &tRc , pt )) { iSelectedIndex = iIndex ; break ; }
                } _endwith;
            }
            if (iOldSel != iSelectedIndex) { SetUpdate(); }
            
            //start of the dragging position
            if (iSelectedIndex >= 0) {                
                iDragStartX = (short)LOWORD(lParam)  ; iDragStartY = (short)HIWORD(lParam);
                bDragging = 1; SetCapture( hwnd );
            }
            
            return 0;
        }
        case WM_LBUTTONUP: {       //Button released
            if (bDragging) { SetCapture( NULL ); }
            if (bDragging>1) { iMaxXIdx=-1 ; ScrollUpdate(hwnd,-1,-1); }
            bDragging = 0; return 0;
        }
    }
    
    return DefWindowProc( hwnd ,message , wParam , lParam );
    #undef SetUpdateAsync
    #undef SetUpdate    
    #undef aObject
}
    
void Diagram_Init( HINSTANCE hinstance ) {
    // Setup window class  
    WNDCLASS wcls = {0};
    _with(wcls) {
        w->style         = 0; //CS_HREDRAW | CS_VREDRAW;
        w->lpfnWndProc   = Diagram_WndProc;
        w->cbClsExtra    = 0;
        w->cbWndExtra    = 0;
        w->hInstance     = hinstance;
        w->hIcon         = NULL;
        w->hCursor       = LoadCursor( NULL, IDC_ARROW );
        w->hbrBackground = NULL;
        w->lpszMenuName  = NULL;
        w->lpszClassName = "Diagram";
    } _endwith
    
    if ( !RegisterClass( &wcls ) ) { puts("Failed to register Diagram Control"); }
}