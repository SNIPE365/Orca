typedef struct {
    int32_t iLength,iBuffer;
    char zContent[0];
} ClsStringStruct;

LRESULT fnClsStringHandler( _ClassPrototype ) {
    DiagramObjectStruct* pDiagram = ((DiagramObjectStruct*)pObject)-1;
    _with( *(ClsStringStruct*)pObject; ) {
        switch (message) {
            case WM_PAINT: {
                HDC hdc = hDcBuffer;
                SelectObject( hdc , hCtlFont );
                RECT* pRc = (RECT*)lParam;
                DrawText( hdc , w->zContent , w->iLength , pRc , DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
                break;
            }
            case CM_GenerateCode: { //wParam = BufferRemaining // lParam = (char*)Buffer
                #define emitf(...) iLen += sprintf( pBuffer+iLen , __VA_ARGS__ )
                char* pBuffer = (char*)lParam; int32_t iBufSz = (int32_t)wParam, iLen=0;
                emitf( "  char* %s = \"'" , pDiagram->zName );
                iLen += EmitSafeString( pBuffer+iLen , iBufSz-iLen , w->zContent , w->iLength );
                emitf( "%s" , "'\";\n" );
                strcpy( g_ScratchBuffer , pDiagram->zName );
                return iLen;
                #undef emitf
            }
            case CM_BeginEdit: { //wParam = hCtlEdit // lParam = (POINTS)ptClick
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
            case CM_CancelEdit: { //wParam = hCtlEdit // lParam = (void**)pObject
                break; // nothing todo here
            }
            default:
                break;
        }
    } _endwith;
}
