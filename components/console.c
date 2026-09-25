typedef struct {
    HANDLE hConsole;
} ClsStdOutStruct;

LRESULT fnClsStdoutHandler( _ClassPrototype ) {
    DiagramObjectStruct* pDiagram = ((DiagramObjectStruct*)pObject)-1;
    _with( *(ClsStdOutStruct*)pObject ) {
        switch (message) {
            case WM_PAINT: {
                /*
                    HDC hdc = hDcBuffer;
                    SelectObject( hdc , hCtlFont );
                    RECT* pRc = (RECT*)lParam;
                    DrawText( hdc , w->zContent , w->iLength , pRc , DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
                    break;
                */
            }
            case CM_GenerateCode: { //wParam = BufferRemaining // lParam = (char*)Buffer
                #define emitf(...) iLen += sprintf( pBuffer+iLen , "  " __VA_ARGS__ )
                char* pBuffer = (char*)lParam; int32_t iBufSz = (int32_t)wParam, iLen=0;
                emitf( "puts(%s);\r\n" , g_ScratchBuffer );
                return iLen;
                #undef emitf
            }
            case CM_BeginEdit:
            case CM_EndEdit:
                break;
            default:
                break;
        }
    } _endwith;
    return 0;
}
