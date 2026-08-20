typedef struct {
    int32_t iLength,iBuffer;
    char zContent[0];
} ClsStringStruct;

LRESULT fnClsStringHandler( _ClassPrototype ) {
    HDC hdc = hDcBuffer;
    SelectObject( hdc , hCtlFont );
    _with( *(ClsStringStruct*)pObject ) {
        DrawText( hdc , w->zContent , w->iLength , pRc , DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX );
    } _endwith;
}
