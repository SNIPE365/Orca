// this is included as part of Diagram.c WndProc function for easy context sharing

static char g_ScratchBuffer[65536];

// realloc the content of a DiagramObjectStruct, preserving the object header
// whenever an object receives a message that will lead to realloc
// it gets an opaque pointer (could we call that a handle?)
// that is passed to resize the object using this function
void* objReallocContent( void** pObject , uint32_t iLength ) {
    _auto pObj = (DiagramObjectStruct**)pObject;
    *pObj = realloc( *pObj , sizeof(*pObj)+iLength );
    return *pObj;
}

#include "string.c"
#include "console.c"

LRESULT fnClsInvalidHandler( _ClassPrototype ) { return 0; }
void initComponents() {

    // initialize object type info handlers (can't declare statically because they are nested functions)
    #define _SetHandler( _xGroupx , _Class , _xNamex , _xColorx , _xInPinsx , _xOutPinsx , _xExecPinsx )  g_ClassInterface[ iIdx++ ].pfHandlerProc = fn##_Class##Handler;
    int iIdx=0;
    _ForEachBuiltinClassID( _SetHandler );
    #undef _SetHandler

    //initialize brushes for object palette theme
    for (int N=0 ; N < _countof(g_ClassInterface) ; N++) {
        hbObject[N] = CreateSolidBrush( g_ClassInterface[N].uColor );
    }
}
