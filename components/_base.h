//this is included as part of Diagram.c

//all of the built-in classes have their metadata defined here

#define _ForEachBuiltinClassID( _Do ) \
    /*   ClassGroup      Class        Name                Color    */\
    _Do( cgrpInvalid   , ClsInvalid , "Invalid"         , 0xFF00FF ) \
    _Do( cgrpContainer , ClsString  , "String"          , 0xFF8844 ) \
    _Do( cgrpDevice    , ClsStdout  , "Standard Output" , 0x55FF55 )

//Object class is actual component (some built-in, and others added dynamically
//but the non built-in ones are added dynamically and indexed for fast lookup
//so when saving to the disk those would need to be stored as a string or an UUID
#define _DeclEnum( _xGroupx , _Class , _xNamex , _xColorx ) id##_Class,
typedef enum {
    _ForEachBuiltinClassID( _DeclEnum )
} ObjectClassID;
#undef _DeclEnum

#define _DeclAsArray( _Group , _xClassx , _Name , _Color ) { .bGroup = _Group , .pzName = _Name , .uColor = _Color },
static ClassInterfaceStruct g_ClassInterface[] = {
    _ForEachBuiltinClassID( _DeclAsArray )
};
#undef _DeclAsArray
