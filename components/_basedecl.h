// this is included as part of Diagram.c WndProc function for easy context sharing

static char g_ScratchBuffer[65536];

// realloc the content of a DiagramObjectStruct, preserving the object header
// whenever an object receives a message that will lead to realloc
// it gets an opaque pointer (could we call that a handle?)
// that is passed to resize the object using this function
void* objReallocContent( void** pObject , uint32_t iLength ) {
    _auto pObj = (DiagramObjectStruct**)pObject;
    size_t uSize = sizeof(**pObj)+iLength;
    _auto p = *pObj;
    *pObj = realloc( *pObj , uSize+16 );
    printf("%p -> %p(%i)\n" , p, *pObj, uSize);
    return ((*pObj)->Content);
}

// emit a safe string to the buffer, escaping any quotes or backslashes
int32_t EmitSafeString( char* pBuffer , int32_t iBufSz , const char* zContent , int32_t iLength ) {
    int32_t iStart = 0, iLen = 0;
    for (int i=0 ; i<iLength ; i++ ) {
        char bChar = zContent[i];
        switch (bChar) {
        case 0 ... 31:
            switch (bChar) {
                case '\t': bChar = 't'; break;
                case '\n': bChar = 'n'; break;
                case '\r': bChar = 'r'; break;
                case '\f': bChar = 'f'; break;
                case '\v': bChar = 'v'; break;
                default: bChar = '?'; break;
            }
            __fallthrough;
        case '\"':
        case '\\':
            if (i!=iStart) { memcpy( pBuffer+iLen , zContent+iStart , (i-iStart) ); }
            iLen += (i-iStart); iStart = i+1;
            pBuffer[iLen++] = '\\';
            pBuffer[iLen++] = bChar;
        }
    }
    if (iStart < iLength) {
        //printf("iStart=%d iLength=%d\n" , iStart, iLength);
        memcpy( pBuffer+iLen , zContent+iStart , iLength-iStart );
        iLen += iLength-iStart;
    }
    return iLen;
}

#include "string.c"
#include "console.c"

LRESULT fnClsInvalidHandler( _ClassPrototype ) { return 0; }
void initComponents() {

    // initialize object type info handlers (can't declare statically because they are nested functions)
    #define _SetHandler( _xGroupx , _Class , _xNamex , _xColorx , _xInPinsx , _xOutPinsx , _xExecPinsx , _xTemplatex , _xFlagsx ) \
        (g_ClassInterface[ iIdx++ ].pfHandlerProc = fn##_Class##Handler)( NULL , CM_GetInfo , 0,0 );
    // set handler and call CM_GetInfo to initialize
    int iIdx=0;
    _ForEachBuiltinClassID( _SetHandler );
    #undef _SetHandler

    //initialize brushes for object palette theme
    for (int N=0 ; N < _countof(g_ClassInterface) ; N++) {
        hbObject[N] = CreateSolidBrush( g_ClassInterface[N].uColor );
    }
}
