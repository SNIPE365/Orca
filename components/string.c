typedef struct {
    int32_t iLength,iBuffer;
    char zContent[0];
} ClsStringStruct;

LRESULT fnClsStringHandler( _ClassPrototype ) {
    _with( *(ClsStringStruct*)pObject ) {
        switch (message) {
            case WM_PAINT: {
                HDC hdc = hDcBuffer;
                SelectObject( hdc , hCtlFont );
                RECT* pRc = (RECT*)lParam;
                DrawText( hdc , w->zContent , w->iLength , pRc , DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
                break;
            }
            case CM_GenerateCode:
                printf( "'%i' = '%s'\n" , sizeof(DiagramObjectStruct) , w->zContent );
                //w->zName
                return 0;
            default:
                break;
        }
    } _endwith;
}
