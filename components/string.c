typedef struct {
    int32_t iLength,iBuffer;
    char zContent[0];
} ClsStringStruct;

LRESULT fnClsStringHandler( _ClassPrototype ) {
    DiagramObjectStruct* pDiagram = ((DiagramObjectStruct*)pObject)-1;
    _with( *(ClsStringStruct*)pObject ) {
        switch (message) {
            case WM_PAINT: {
                HDC hdc = hDcBuffer;
                SelectObject( hdc , hCtlFont );
                RECT* pRc = (RECT*)lParam;
                DrawText( hdc , w->zContent , w->iLength , pRc , DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
                break;
            }
            case CM_GenerateCode: { //wParam = BufferRemaining // lParam = (char*)Buffer
                #define emitf(...) iLen += sprintf( pBuffer+iLen , "  " __VA_ARGS__ )
                char* pBuffer = (char*)lParam; int32_t iBufSz = (int32_t)wParam, iLen=0;
                emitf( "char* %s = \"%s\";\r\n" , pDiagram->zName , w->zContent );
                strcpy( g_ScratchBuffer , pDiagram->zName );
                return iLen;
                #undef emitf
            }
            case CM_BeginEdit: { //wParam = hCtlEdit // lParam = (POINT)ptClick
                HANDLE hCtlEdit = (HANDLE)wParam;
                POINT ptClick = *(POINT*)&lParam;
                RECT tRect; GetClientRect( hCtlEdit , &tRect );
                SetWindowText( hCtlEdit , w->zContent );
                SendMessage( hCtlEdit , EM_SETSEL , 0 , -1 );
            }
            case CM_EndEdit:   { //wParam = hCtlEdit // lParam = (void**)pObject
                HANDLE hCtlEdit = (HANDLE)wParam;
                void** ppObject = (void**)lParam;
                int32_t iLength = GetWindowTextLength( hCtlEdit );
                pObject = objReallocContent( ppObject , sizeof(*pObject)+iLength+1 );
                GetWindowText( hCtlEdit , w->zContent , iLength+1 );
            }
            default:
                break;
        }
    } _endwith;
}
