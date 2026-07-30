typedef struct {
    int iX,iY,iW,iH;
    char zCaption[15];
    unsigned char bColor;
} DiagramObjectStruct;

static CALLBACK LRESULT Diagram_WndProc ( HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam ) {
    
    typedef enum {
        DIM_BASE = WM_USER,
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
    
    #define SetUpdate() if (bDrawn) { bDrawn=0 ; SetTimer( hwnd , dmtRedraw , 10 , NULL ); }
    
    static int iObjCount=0, iObjMaxCount=256 , iMaxX=0, iMaxY=0, iViewX=0, iViewY=0;
    static int iStartIdx=0, iEndIdx=-1, iSelectedIndex=-1;
    static int iMouseX=0,iMouseY=0;
    
    static DiagramObjectStruct* pObjects = NULL;
    static DiagramObjectStruct** pOrder = NULL;
        
    switch (message) {
        case WM_ERASEBKGND: { return 1; }
        case WM_SETCURSOR: {
            const POINT pt = { iMouseX , iMouseY };
            for ( int iIndex = iStartIdx ; iIndex<=iEndIdx ; iIndex++ ) {                
                _with( *pOrder[iIndex] ) {
                    const RECT tRc = { .left = w->iX-iViewX , .top = w->iY-iViewY , .right = w->iX-iViewX+w->iW , .bottom = w->iY-iViewY+w->iH };                                        
                    if (PtInRect( &tRc , pt )) { SetCursor( LoadCursor( NULL , IDC_HAND ) ) ; return 0; }
                } _endwith;
            }            
            break;
        }
        case WM_MOUSEMOVE: {       //Mouse moved in the control
            iMouseX = (short)LOWORD(lParam);  // horizontal position of cursor 
            iMouseY = (short)HIWORD(lParam);  // vertical position of cursor
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
            switch (nScrollCode) {
                case SB_TOP:           { tInfo.nPos = tInfo.nMin; break; }
                case SB_BOTTOM:        { tInfo.nPos = tInfo.nMax; break; }
                case SB_ENDSCROLL:     { break; }
                case SB_LINEDOWN:      { tInfo.nPos = min( tInfo.nPos+24 , tInfo.nMax ); break; }
                case SB_LINEUP:        { tInfo.nPos = max( tInfo.nPos-24 , tInfo.nMin ); break; }
                case SB_PAGEDOWN:      { tInfo.nPos = min( tInfo.nPos+tInfo.nPage , tInfo.nMax ); break; }
                case SB_PAGEUP:        { tInfo.nPos = max( tInfo.nPos-tInfo.nPage , tInfo.nMin ); break; }
                case SB_THUMBPOSITION: { break; }
                case SB_THUMBTRACK:    { tInfo.nPos = tInfo.nTrackPos; break; }                
            }
            tInfo.fMask = SIF_POS;            
            if (!SetScrollInfo( hwnd , SB_ , &tInfo , true )) {
                puts("Failed to set diagram scroll info");
            }
            SetUpdate();
            return 0;
        }        
        case WM_TIMER: {           //TIMER events (REDRAW!)            
            if (bDrawn==1) { return 0; }            
            HDC hdc = hDcBuffer;
            RECT tRc = {0,0,iBufWid,iBufHei};            
                        
            iViewX = GetScrollPos( hwnd , SB_HORZ );
            iViewY = GetScrollPos( hwnd , SB_VERT );
            SetBrushOrgEx( hdc , 1 , 4-(iViewY & 7) , NULL );
            SetBrushOrgEx( hdc , 4 , 4-(iViewY & 7) , NULL );
            FillRect( hdc , &tRc , hbBackGrid );
            SetBkMode( hdc , TRANSPARENT );
            
            //check if there's previous items that are visible (caused by moving or scroll up)
            if (!pOrder) { return 0; }   
            
            puts("drawing start");
            
            for ( ; iStartIdx>0 ; iStartIdx-- ) {
                _with( *pOrder[iStartIdx-1] ) {
                    if ((w->iY+w->iH-iViewY) < 0) { break; }
                } _endwith;
            }
            
            int iIndex;
            for ( iIndex=iStartIdx ; (iIndex < iObjCount) ; iIndex++) {
                _with( *pOrder[iIndex] ) {
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
                        RoundRect( hdc , tObjRc.left-4 , tObjRc.top-4 , tObjRc.right+4 , tObjRc.bottom+4 , 16 , 16 );
                        SelectObject( hdc , hOldPen ); SelectObject( hdc , hOldBrush );
                    }                        
                    
                    if (iIndex < (iObjCount-1)) {
                        iPosX += w->iW/2; iPosY += w->iH;
                        MoveToEx( hdc , iPosX , iPosY , NULL ); LineTo( hdc , iPosX , iPosY+8 );
                    }
                    
                    
                    
                } _endwith;
            }
            
            iEndIdx=(iIndex < iObjCount ? iIndex-1 : iObjCount-1) ; bDrawn = 1; 
            puts("drawing end");
            InvalidateRect( hwnd , NULL , true ); UpdateWindow( hwnd );            
            if (wParam) { KillTimer(hwnd,wParam); }
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
            SCROLLINFO tInfoH = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL , .nPage = nWid , .nMin = 0 , .nMax = iMaxX };
            SetScrollInfo( hwnd , SB_HORZ , &tInfoH , true );            
            SCROLLINFO tInfoV = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL , .nPage = nHei , .nMin = 0 , .nMax = iMaxY };
            SetScrollInfo( hwnd , SB_VERT , &tInfoV , true );
            bUpdateScroll = 1;
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
            tInfo.nPos = min( tInfo.nPos+zDelta/-2 , tInfo.nMax );
            tInfo.fMask = SIF_POS;
            SetScrollInfo( hwnd , SB_VERT , &tInfo , true );
            SetUpdate();
            return 0;
        }
        case WM_CREATE: {          //Initialize control
            EnableScrollBar( hwnd , SB_HORZ , ESB_DISABLE_BOTH );
            EnableScrollBar( hwnd , SB_VERT , ESB_DISABLE_BOTH );            
            hbBack = CreateSolidBrush( cBack );            
            hbBackGrid = CreateHatchBrush( HS_CROSS , cGrid );
            hpSelected = CreatePen( PS_SOLID , 4 , cSelected );

            for (int N=0 ; N < 256 ; N++) { //initialize brushes for object palette theme
                if (cObject[N] < 0) { break; }
                hbObject[N] = CreateSolidBrush( cObject[N] );
            }
            
            pObjects = malloc(iObjMaxCount*sizeof(DiagramObjectStruct));
            pOrder = malloc(iObjMaxCount*sizeof(DiagramObjectStruct*));
            
            // generate semi-random objects for initial tests
            int iPosY=0, iPosX=0, iBigRow ; iObjCount = iObjMaxCount;
            for (int N=0 ; N < iObjCount ; N++ ) {
                pOrder[N] = &pObjects[N];
                if (!iPosX) { iPosX = (rand() % 256) & (~7); iBigRow = 0; }
                _with( *pOrder[N] ) {
                    w->iX = iPosX;
                    w->iW = (48+(rand() % 128)) & (~7);
                    w->iY = iPosY; 
                    w->iH = (32+(rand() % 64)) & (~7);
                    w->bColor = rand() % 6;
                    sprintf(w->zCaption , "Obj%i", N+1 );                    
                    
                    if ((w->iX+w->iW) > iMaxX) { iMaxX = (w->iX+w->iW); }
                    if ((w->iY+w->iH) > iMaxY) { iMaxY = (w->iY+w->iH); }
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
        case WM_LBUTTONDOWN: {     //Button pressed
            int iOldSel = iSelectedIndex ; iSelectedIndex = -1;
            //printf("%i to %i\n",iStartIdx,iEndIdx);
            for ( int iIndex = iStartIdx ; iIndex<=iEndIdx ; iIndex++ ) {
                _with( *pOrder[iIndex] ) {
                    const RECT tRc = { .left = w->iX-iViewX , .top = w->iY-iViewY , .right = w->iX-iViewX+w->iW , .bottom = w->iY-iViewY+w->iH };                    
                    const POINT pt = { (short)LOWORD(lParam) , (short)HIWORD(lParam) };                    
                    if (PtInRect( &tRc , pt )) { iSelectedIndex = iIndex ; break ; }
                } _endwith;
            }
            if (iOldSel != iSelectedIndex) { SetUpdate(); }
            return 0;
        }
    }
    
    return DefWindowProc( hwnd ,message , wParam , lParam );
    #undef SetUpdate
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