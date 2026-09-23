#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#include "..\modules\menuActions.c"

#ifndef main_module
  _err( "Compile from " TOSTRING(main_module) ".c" )
#endif

// *********** Menu Definition ****************
#define ForEachMenuEntry( __Entry , __SubMenu , __EndSubMenu , __Separator ) \
   __SubMenu( "&File" ) \
     /*__Entry( meFile_New      , "&New"              , _Ctrl        , VK_N , &File_New    ) \
     __Entry( meFile_open     , "&Open"             , _Ctrl        , VK_O , &File_Open   ) \
     __Entry( meFile_Save     , "&Save"             , _Ctrl        , VK_S , &File_Save   ) \
     __Entry( meFile_SaveAs   , "Save &As"          , _Ctrl+_Shift , VK_S , &File_SaveAs ) \
     __Entry( meFile_Close    , "&Close"            , _Ctrl        , VK_W , &File_Close  ) \
     __Separator() \
      __Entry( meFile_Import  , "&Import"           , _Ctrl        , VK_I , &File_Import ) \
      __Entry( meFile_Export  , "&Export"           , _Ctrl+_Shift , VK_I , &File_Export ) \
     __Separator() */\
     __Entry( meFile_Exit     , "&Quit" "\tAlt+F4"  , _Ctrl        , VK(_Q) , &File_Exit   ) \
   __EndSubMenu() \
   __SubMenu( "&Project" ) \
      __Entry( meProject_build     , "&Build"       , _Ctrl        , VK(_B)  , Project_Build   ) \
      __Entry( meProject_Qbuild    , "&Quick Build" ,              , VK(_F6) , Project_Build   ) \
   __EndSubMenu() \
   __SubMenu( "*[" __TIMESTAMP__ "]" ) \
   __EndSubMenu()
   /*__SubMenu( "&Edit" ) \
      __Entry( meEdit_Undo    , "&Undo"  "\tCtrl+Z"  ,              ,      , &Edit_Undo ) \
      __Entry( meEdit_Redo    , "&Redo"              , _Ctrl+_Shift , VK(_Z) , &Edit_Redo ) \
      __Separator() \
      __Entry( meEdit_Find    , "&Find"              , _Ctrl        , VK(_F) , &Edit_Find ) \
      __Entry( meEdit_Replace , "Rep&lace"           , _Ctrl        , VK(_H) , &Edit_Replace ) \
      __Separator() \
      __Entry( meEdit_SelAll  , "&Select All"        , _Ctrl        , VK(_A) , &Edit_SelectAll  ) \
      __Separator() \
      __Entry( meEdit_Cut     , "C&ut"   "\tCtrl+X"  ,              ,      , &Edit_Cut  ) \
      __Entry( meEdit_Copy    , "&Copy"  "\tCtrl+C"  ,              ,      , &Edit_Copy ) \
      __Entry( meEdit_Paste   , "&Paste" "\tCtrl+V"  ,              ,      , &Edit_Paste) \
      __Separator() \
      __Entry( meCode_Build   , "&Build"             , 0            , VK(_F6) , &Button_Compile ) \
      __Entry( meCode_Clear   , "Cl&ear output"      , _Ctrl+_Shift , VK(_B) , &Code_ClearOutput ) \
   __EndSubMenu()*/
//-------------------------------------------------------------------------------------------

#if 1 //key definitions
    #define _Shift FSHIFT
    #define _Ctrl  FCONTROL
    #define _Alt   FALT
    #define Dummy()
    #define VK_A 'A'
    #define VK_B 'B'
    #define VK_C 'C'
    #define VK_D 'D'
    #define VK_E 'E'
    #define VK_F 'F'
    #define VK_G 'G'
    #define VK_H 'H'
    #define VK_I 'I'
    #define VK_J 'J'
    #define VK_K 'K'
    #define VK_L 'L'
    #define VK_M 'M'
    #define VK_N 'N'
    #define VK_O 'O'
    #define VK_P 'P'
    #define VK_Q 'Q'
    #define VK_R 'R'
    #define VK_S 'S'
    #define VK_T 'T'
    #define VK_U 'U'
    #define VK_V 'V'
    #define VK_W 'W'
    #define VK_X 'X'
    #define VK_Y 'Y'
    #define VK_Z 'Z'
#endif

#define EnumEntry( _Name , _p... ) _Name,
#define MayEnumEntry( _p... ) EnumEntry(_p)
#define MayEnumSubMenu( _s , _name... ) _name
    typedef enum {
    meFirst = 1000,
    ForEachMenuEntry( MayEnumEntry , MayEnumSubMenu , Dummy , Dummy )
    meLast
    } MenuEntries;
#undef EnumEntry
#undef MayEnumEntry
#undef MayEnumSubMenu

static void* menu_AddSubMenu( void* hMenu , char* pzText , int iID /* = 0 */ ) {
    if (!IsMenu(hMenu)) { return NULL; }
    _auto hResult = CreatePopupMenu();
    //AppendMenu( hMenu , MF_POPUP | MF_STRING , (UINT_PTR)(hResult) , pzText )
    MENUITEMINFOA tItem = { sizeof(MENUITEMINFO) };
    _with(tItem) {
        w->fMask      = MIIM_SUBMENU | MIIM_ID | MIIM_STATE | MIIM_TYPE;
        w->hSubMenu   = hResult ; w->wID    = iID;
        w->dwTypeData = pzText  ; w->fState = MFS_ENABLED; w->fType = MFT_STRING;
        if (pzText[0]=='*') { w->dwTypeData = pzText+1; w->fState |= MFS_GRAYED ; w->fType |= MFT_RIGHTJUSTIFY ; }
    } _endwith
    InsertMenuItemA( hMenu , -1 , true , &tItem );
    if ( ( hMenu==g_WndMenu ) && (_CTL(wcMain)) ) { DrawMenuBar( _CTL(wcMain) ); }
    return hResult;
} // menu_AddSubMenu()
static int menu_MenuAddEntry( void* hMenu , int iID /* = 0 */ , char* pzText /* = NULL */ , int iModifiers /* = 0 */ , int iAccelerator /* = 0 */ , void* pEvent /* = NULL */ , int bState /* = 0 */ ) {
    if (!IsMenu(hMenu)) { return -1; }
    MENUITEMINFOA tItem = { sizeof(MENUITEMINFO) };
    tItem.fMask      = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_TYPE;
    tItem.fType      = ( pzText ? (( bState & MFT_RADIOCHECK ) ? MFT_RADIOCHECK : MFT_STRING ) : MFT_SEPARATOR );
    tItem.fState     = bState & (~MFT_RADIOCHECK);
    tItem.wID        = iID;
    tItem.dwItemData = (LONG_PTR)pEvent;
    if (pzText) {
        char szText[256]; int iPos=0;
        iPos = sprintf( szText , "%s" , pzText );
        if (iAccelerator && (!strchr(pzText,'\t'))) {
            szText[iPos++] = '\t';
            int uScan = MapVirtualKey( iAccelerator , 0 ) << 16;
            if (iModifiers & FCONTROL) { iPos += sprintf( szText + iPos , "Ctrl+" ); }
            if (iModifiers & FSHIFT) { iPos += sprintf( szText + iPos , "Shift+" ); }
            if (iModifiers & FALT) { iPos += sprintf( szText + iPos , "Alt+" ); }
            //todo check if extended key flag is required?
            GetKeyNameText( uScan , szText + iPos , 256 - iPos );
        }
        tItem.dwTypeData = szText;
    } else {
        tItem.dwTypeData = NULL;
    }
    InsertMenuItemA( hMenu , 0xFFFFFFFF , true , &tItem );
    //DrawMenuBar( g_GfxWnd )
    return iID;
} // menu_MenuAddEntry()
//MFS_CHECKED , MFS_DEFAULT , MFS_DISABLED , MFS_ENABLED , MFS_GRAYED , MFS_HILITE , MFS_UNCHECKED , MFS_UNHILITE
static int menu_MenuState( void* hMenu , int iID , int bState ) {
  if (!IsMenu(hMenu)) { return -1; }
  MENUITEMINFO tItem =  { sizeof(MENUITEMINFO) , MIIM_STATE , .fState = bState };
  SetMenuItemInfo( hMenu , iID , false , &tItem );
  return bState;
} //menu_MenuState()
static int menu_MenuText( void* hMenu , int iID , char* pzText ) {
  if (!IsMenu(hMenu)) { return -1; }
  MENUITEMINFO tItem = { sizeof(MENUITEMINFO) , MIIM_TYPE };
  GetMenuItemInfoA( hMenu , iID , false , &tItem );
  tItem.dwTypeData = pzText;
  SetMenuItemInfoA( hMenu , iID , false , &tItem );
  return strlen(pzText);
} //menu_MenuText()
static bool menu_IsChecked( int iID ) {
  return (GetMenuState( g_WndMenu , iID , MF_BYCOMMAND ) & MF_CHECKED ) != 0;
} //menu_IsChecked()
static void menu_Trigger( int iID ) {
  SendMessage( _CTL(wcMain),WM_MENUSELECT,iID,(LPARAM)(g_WndMenu));
  SendMessage( _CTL(wcMain) , WM_COMMAND , iID , 0 );
} //menu_Trigger()

static HACCEL menu_CreateAcceleratorTable(void) {

    //generate a compile time array of atAccel depending on the menu entries
    //if accelerator is empty, the entry is skipped otherwise _MenuAcc##_Accelerator is used
    //which leads to _MenuRowGen1() writing the code or _MenuRowGen0() skipping it
    //if anything other than VK(...) is used in the accelerator entry this will fail
    //
    #define _SubMenu( _sText... )
    #define _EndSubMenu()
    #define _Separator()
    #define _MenuWrap(...) _MenuRow(__VA_ARGS__)
    #define _MenuAcc 0,0            //when accelerator is empty
    #define _MenuAccVK(_N) 1,VK##_N //when accelerator is VK(_N)
    #define _MenuRow( _fVirt , _valid , _key , _cmd ) _MenuRowGen##_valid( _fVirt , _key , _cmd )
    #define _MenuRowGen1( _fVirt , _key , _cmd ) {.fVirt=_fVirt,.key=_key,.cmd=_cmd},
    #define _MenuRowGen0( _fVirt , _key , _cmd ) /* skipped */
    #define _Entry( _idName , _Text , _Modifiers , _Accelerator , _Callback... ) \
        _MenuWrap( FVIRTKEY|_Modifiers+0 , _MenuAcc##_Accelerator , _idName )

    ACCEL atAccel[] = {
        ForEachMenuEntry( _Entry ,  _SubMenu , _EndSubMenu , _Separator )
    };

    /*
        for (int i=0 ; i<_countof(atAccel) ; i++) {
            _with( atAccel[i] );
                printf("#%i:%i[%i] = %i(%c)\n", i, (int)w->cmd,(int)w->fVirt,(int)w->key,(int)w->key);
            _endwith;
        }
    */

    return CreateAcceleratorTable( atAccel , _countof(atAccel) );

    #undef _SubMenu
    #undef _EndSubMenu
    #undef _Separator
    #undef _Entry
    #undef _MenuAcc
    #undef _MenuAccVK
    #undef _MenuRow
    #undef _MenuRowGen1
    #undef _MenuRowGen0
    #undef _MenuWrap
}

static HMENU menu_CreateMainMenu(void) {
    #define _SubMenu( _sText... ) \
    { \
      _auto hMenu = menu_AddSubMenu( hMenu , _sText , 0 ); \

    #define _EndSubMenu() }
    #define _Separator() menu_MenuAddEntry( hMenu , 0 , NULL , NULL , 0 );
    #define VK(_N) (VK##_N)

    #define _Entry( _idName , _Text , _Modifiers , _Accelerator , _Callback... ) \
        { \
            _const _sText2 = _Text ; \
            menu_MenuAddEntry( hMenu , _idName , _sText2 , _Modifiers+0 , _Accelerator+0 , _Callback+0 , 0 ); \
        }

    _auto hMenu = CreateMenu() ; g_WndMenu = hMenu;

    ForEachMenuEntry( _Entry ,  _SubMenu , _EndSubMenu , _Separator )

    #undef _SubMenu
    #undef _EndSubMenu
    #undef _Separator
    #undef _Entry
    #undef VK

    return hMenu;
} //menu_CreateMainMenu()
