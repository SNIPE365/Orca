#define ThisClass ClsString

typedef struct {
    int32_t iLength,iBuffer;
    char zContent[0];
} ClsStringStruct;

LRESULT fnClsStringHandler( _ClassPrototype ) {
    DiagramObjectStruct* pDiagram = ((DiagramObjectStruct*)pObject)-1;
    _with( *(ClsStringStruct*)pObject; ) {
        switch (message) {
            case WM_PAINT:        { //lParam = (RECT*)pRc
                HDC hdc = hDcBuffer;
                SelectObject( hdc , hCtlFont );
                RECT *pRc = (RECT*)lParam;
                RECT RcCalc=*pRc;
                int iFlags = DT_NOPREFIX | DT_CENTER  ;
                DrawText( hdc , w->zContent , w->iLength , &RcCalc , iFlags | DT_CALCRECT );
                int iWid = RcCalc.right - RcCalc.left, iHei = RcCalc.bottom - RcCalc.top;
                int iMaxWid = pRc->right - pRc->left, iMaxHei = pRc->bottom - pRc->top;
                if (iWid > iMaxWid) { iWid = iMaxWid; iFlags |= DT_END_ELLIPSIS; }
                if (iHei > iMaxHei) { iHei = iMaxHei; iFlags |= DT_END_ELLIPSIS; }
                pRc->left += iMaxWid/2; pRc->top += iMaxHei/2;
                pRc->right = pRc->left + iWid/2; pRc->bottom = pRc->top + iHei/2;
                pRc->left -= iWid/2; pRc->top -= iHei/2;
                DrawText( hdc , w->zContent , w->iLength , pRc , iFlags );
                break;
            }
            case CM_GetInfo:      { //lParam = (ClassInfoStruct*)pOptOutInfo
                __Internal_ClassInfoStruct(ThisClass);
                break;
            }
            case CM_GenerateCode: { //wParam = BufferRemaining // lParam = (char*)Buffer
                #define emitf(...) iLen += sprintf( pBuffer+iLen , __VA_ARGS__ )
                char* pBuffer = (char*)lParam; int32_t iBufSz = (int32_t)wParam, iLen=0;
                emitf( "  char* %s = \"" , pDiagram->zName );
                iLen += EmitSafeString( pBuffer+iLen , iBufSz-iLen , w->zContent , w->iLength );
                emitf( "%s" , "\";\n" );
                strcpy( g_ScratchBuffer , pDiagram->zName );
                return iLen;
                #undef emitf
            }
            case CM_BeginEdit:    { //wParam = hCtlEdit // lParam = (POINTS)ptClick
                HANDLE hCtlEdit = (HANDLE)wParam;
                POINTS ptClick = *(POINTS*)&lParam;
                RECT tRect; GetClientRect( hCtlEdit , &tRect );
                SetWindowText( hCtlEdit , w->zContent );
                SendMessage( hCtlEdit , EM_SETSEL , -2 , -2 );
                return 1;
            }
            case CM_EndEdit:   { //wParam = hCtlEdit // lParam = (void**)pObject
                HANDLE hCtlEdit = (HANDLE)wParam;
                void** ppObject = (void**)lParam;
                int32_t iLength = GetWindowTextLength( hCtlEdit );
                typeof(w) pw = objReallocContent( ppObject , sizeof(*pObject)+iLength+1 );
                pw->iLength = iLength; pw->iBuffer = iLength+1;
                GetWindowText( hCtlEdit , pw->zContent , pw->iBuffer );
                break;
            }
            case CM_CancelEdit:   { //wParam = hCtlEdit // lParam = (void**)pObject
                break; // nothing todo here
            }
            default:
                break;
        }
    } _endwith;
}

#undef ThisClass
