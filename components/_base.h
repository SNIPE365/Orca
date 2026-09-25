//this is included as part of Diagram.c

//all of the built-in classes have their metadata defined here

#define _ForEachBuiltinClassID( _Do ) \
    /*   ClassGroup      Class        Name                Color    Input Output Exec*/\
    _Do( cgrpInvalid   , ClsInvalid , "Invalid"         , 0xFF00FF , 0 , 0 , 0 ) \
    _Do( cgrpContainer , ClsString  , "String"          , 0xFF8844 , 0 , 1 , 0 ) \
    _Do( cgrpDevice    , ClsStdout  , "Standard Output" , 0x55FF55 , 1 , 0 , 0 )

//Object class is actual component (some built-in, and others added dynamically
//but the non built-in ones are added dynamically and indexed for fast lookup
//so when saving to the disk those would need to be stored as a string or an UUID
#define _DeclEnum( _xGroupx , _Class , _xNamex , _xColorx , _xInx , _xOutx , _xExecx ) id##_Class,
typedef enum {
    _ForEachBuiltinClassID( _DeclEnum )
} ObjectClassID;
#undef _DeclEnum

typedef enum {
    CM_GenerateCode = WM_USER+1,
    CM_BeginEdit,
    CM_EndEdit
} ClassHandlerCommand;

#define _DeclAsArray( _Group , _xClassx , _Name , _Color , _In , _Out , _Exec ) { .bGroup = _Group , .pzName = _Name , .uColor = _Color , .bInPins = _In , .bOutPins = _Out , .bExecPins = _Exec },
static ClassInterfaceStruct g_ClassInterface[] = {
    _ForEachBuiltinClassID( _DeclAsArray )
};
#undef _DeclAsArray
