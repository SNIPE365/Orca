typedef enum {
    DIM_BASE = WM_USER,
    /* PRIVATE ONES */
    DIM_CREATE_BUFFER,    
} DiagramEnum;
typedef enum {
    dmtRedraw = 1,
} DiagramTimers;

static CALLBACK LRESULT Diagram_WndProc ( HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam ) {
    static HBITMAP hBmBuffer;
    static HDC hDcBuffer;
    static int iBufWid,iBufHei,iDrawn=1;
    _const cBack=0xFFFFFF ; _const cObject=0xFF8844 ;
    static HBRUSH hbBack , hbObject ;
    
    #define SetUpdate() if (iDrawn) { iDrawn=0 ; SetTimer( hwnd , dmtRedraw , 10 , NULL ); }
    
    _const NumObjs = 33;
    _const ObjHei = 55;
    
    switch (message) {
        case WM_ERASEBKGND: { return 1; }
        case WM_PAINT: {           //Update window from bitmap if ready
            if (iDrawn <= 0) { ValidateRect( hwnd , NULL ); return 0; }            
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
        case WM_VSCROLL: {
            int nScrollCode = (int)LOWORD(wParam); // scroll bar value
            SCROLLINFO tInfo = { .fMask = SIF_ALL };
            GetScrollInfo( hwnd , SB_VERT , &tInfo );
            switch (nScrollCode) {
                case SB_TOP:           { tInfo.nPos = tInfo.nMin; break; }
                case SB_BOTTOM:        { tInfo.nPos = tInfo.nMax; break; }
                case SB_ENDSCROLL:     { break; }
                case SB_LINEDOWN:      { tInfo.nPos = min( tInfo.nPos+ObjHei , tInfo.nMax ); break; }
                case SB_LINEUP:        { tInfo.nPos = max( tInfo.nPos-ObjHei , tInfo.nMin ); break; }
                case SB_PAGEDOWN:      { tInfo.nPos = min( tInfo.nPos+tInfo.nPage , tInfo.nMax ); break; }
                case SB_PAGEUP:        { tInfo.nPos = max( tInfo.nPos-tInfo.nPage , tInfo.nMin ); break; }
                case SB_THUMBPOSITION: { break; }
                case SB_THUMBTRACK:    { tInfo.nPos = tInfo.nTrackPos; break; }                
            }
            tInfo.fMask = SIF_POS;
            SetScrollInfo( hwnd , SB_VERT , &tInfo , true );            
            SetUpdate();
            return 0;
        }            
        case WM_TIMER: {           //TIMER events (REDRAW!)            
            if (iDrawn==1) { return 0; }            
            HDC hdc = hDcBuffer;
            RECT tRc = {0,0,iBufWid,iBufHei};
            FillRect( hdc , &tRc , GetStockObject( WHITE_BRUSH ) );
            SetBkMode( hdc , TRANSPARENT );            
            
            int iViewY = GetScrollPos( hwnd , SB_VERT ) , iStartIdx = iViewY/ObjHei;
            int iPosY = iStartIdx*ObjHei-iViewY;
            
            for (int iIndex=iStartIdx ; (iPosY < iBufHei) && (iIndex < NumObjs) ; iIndex++) {
                SelectObject( hdc , hbObject );
                RECT tObjRc = {10,iPosY,100,iPosY+50};
                RoundRect( hdc , tObjRc.left , tObjRc.top , tObjRc.right , tObjRc.bottom , 16 , 16 );                
                char zBuff[16]; 
                DrawText( hdc , zBuff , sprintf ( zBuff , "%i" , iIndex ) , &tObjRc , DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
                if (iIndex < (NumObjs-1)) {
                    MoveToEx( hdc , 55 , iPosY+50 , NULL ); LineTo( hdc , 55 , iPosY+55 );
                }
                
                iPosY += ObjHei;
            }
            iDrawn = 1; InvalidateRect( hwnd , NULL , true ); //UpdateWindow( hwnd );            
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
                SelectObject( hdc , (HFONT)SendMessage(hwnd,WM_GETFONT,0,0) );
            }
            hBmBuffer = CreateCompatibleBitmap( hdc , iBufWid , iBufHei );
            DeleteObject( SelectObject( hDcBuffer , hBmBuffer ) );
            SCROLLINFO tInfo = { .fMask = SIF_PAGE | SIF_RANGE , .nPage = nHei , .nMin = 0 , .nMax = NumObjs*ObjHei };            
            SetScrollInfo( hwnd , SB_VERT , &tInfo , true );            
            SetUpdate();
            return 1;
        }
        case WM_CREATE: {          //Initialize control
            EnableScrollBar( hwnd , SB_HORZ , ESB_DISABLE_BOTH );
            hbBack   = CreateSolidBrush( cBack );
            hbObject = CreateSolidBrush( cObject );
            return 1;
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