//this is included as part of Diagram.c

//all of the built-in classes have their metadata defined here

#define _ForEachBuiltinClassID( _Do ) \
    /*                                                              {   Pins   } */\
    /*   ClassGroup      Class        Name                Color     In Out Exec  Template     Flags */\
    _Do( cgrpInvalid   , ClsInvalid , "Invalid"         , 0xFF00FF , 0 , 0 , 0 , "BadBad"      , 0 ) \
    _Do( cgrpContainer , ClsString  , "String"          , 0xFF8844 , 0 , 1 , 0 , "MyString%i"  , 0 ) \
    _Do( cgrpDevice    , ClsStdOut  , "Standard Output" , 0x55FF55 , 1 , 0 , 0 , "StdOut"      , 0 )

//Object class is actual component (some built-in, and others added dynamically
//but the non built-in ones are added dynamically and indexed for fast lookup
//so when saving to the disk those would need to be stored as a string or an UUID
#define _DeclEnum( _xGroupx , _Class , _xNamex , _xColorx , _xInx , _xOutx , _xExecx , _xTemplatex , _xFlagsx ) id##_Class,
typedef enum {
    _ForEachBuiltinClassID( _DeclEnum )
} ObjectClassID;
#undef _DeclEnum

/*
    //?? should we use this to retrieve info... or use the nice global array???
    typedef struct { //Information about a class
        int iMinBytesConstructor;         //how many bytes of constructor data are required
        const char* pzNameTemplate;       //template for the initial object name
    } ClassInfoStruct;
*/

typedef enum { //Handler for all object classes
    CM_GetInfo = WM_USER+1, //Get information about the object
    CM_GenerateCode,        //Generate code for the object
    CM_BeginEdit,           //Start editing the object "display" property
    CM_CancelEdit,          //Cancel editing any property
    CM_EndEdit,             //End editing any propert (update internal data)
} ClassHandlerCommand;

#define _DeclAsArray( _Group , _xClassx , _Name , _Color , _In , _Out , _Exec , _Template , _Flags ) \
{ .bGroup = _Group , .pzName = _Name , .uColor = _Color , \
    .bInPins = _In , .bOutPins = _Out , .bExecPins = _Exec , \
    .pzNameTemplate = _Template , .uFlags = _Flags \
},
// global array with information about all built-in classes
static ClassInterfaceStruct g_ClassInterface[] = {
    _ForEachBuiltinClassID( _DeclAsArray )
};
#undef _DeclAsArray

//macro to return default info for built-in classes
#define _Join( _a , _b ) _a##_b
#define _Stringify( _x ) #_x
#define __Internal_ClassInfoStruct( _ClassName ) \
    _auto pOptOutInfo = (typeof(&g_ClassInterface[0]))lParam; \
    _const clsID = _Join(id,_ClassName); \
    _auto pInfo = &g_ClassInterface[clsID]; \
    pInfo->iMinBytesConstructor = sizeof( _Join(_ClassName,Struct) ); \
    if (pOptOutInfo) { *pOptOutInfo = *pInfo; pInfo = pOptOutInfo; } \
    printf("Initialized: #%i'%s' size=%d\n", clsID, _Stringify(_ClassName), pInfo->iMinBytesConstructor); \
    return (LPARAM)pInfo;
