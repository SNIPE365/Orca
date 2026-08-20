#define _ForEachBuiltinClassGroup( _Do ) \
    /*       ClassGroup      Name  */\
    _Do( cgrpInvalid   , "Invalid" ) \
    _Do( cgrpContainer , "Containers" ) \
    _Do( cgrpFilter    , "Filters" ) \
    _Do( cgrpFunction  , "Functions" ) \
    _Do( cgrpDevice    , "Devices" )

//Object Class Group handles the type of object/component being displayed
//stored internally in the class memory (not in the object/instance)
#define _DeclEnum( _Group , _xNamex ) _Group,
typedef enum {
    _ForEachBuiltinClassGroup( _DeclEnum )
} ObjectClassGroup;
#undef _DeclEnum

typedef struct {
    char*            pzName;
    HANDLE           hITEM;
} ClassGroupStruct;
#define _DeclAsArray( _xGroupx , _Name ) { .pzName = _Name },
static ClassGroupStruct g_atClassGroup[] = {
    _ForEachBuiltinClassGroup( _DeclAsArray )
};
#undef _DeclAsArray

#define _ClassPrototype void* pObject , RECT* pRc , UINT message , WPARAM wParam , LPARAM lParam

//////////////////////////////// Per Class Information ///////////////////////////////////
typedef struct {
    LRESULT (*pfHandlerProc)( void* pObject , RECT* pRc , UINT message , WPARAM wParam , LPARAM lParam );
    char*    pzName;
    uint8_t  bGroup;
    COLORREF uColor;
} ClassInterfaceStruct;
////////////////////////////////////////////////////////////////////////////////////////////////
