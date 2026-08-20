// this is included as part of Diagram.c WndProc function for easy context sharing

#include "string.c"
#include "console.c"

LRESULT fnClsInvalidHandler( _ClassPrototype ) { return 0; }
void initComponents() {

    // initialize object type info handlers (can't declare statically because they are nested functions)
    #define _SetHandler( _xGroupx , _Class , _xNamex , _xColorx )  g_ClassInterface[ iIdx++ ].pfHandlerProc = fn##_Class##Handler;
    int iIdx=0;
    _ForEachBuiltinClassID( _SetHandler );
    #undef _SetHandler

    //initialize brushes for object palette theme
    for (int N=0 ; N < _countof(g_ClassInterface) ; N++) {
        hbObject[N] = CreateSolidBrush( g_ClassInterface[N].uColor );
    }
}
