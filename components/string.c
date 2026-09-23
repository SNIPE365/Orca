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
            default:
                break;
        }
    } _endwith;
}
