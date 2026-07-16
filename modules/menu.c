#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#include "..\modules\menuActions.c"

#ifndef main_module  
  _err( "Compile from " TOSTRING(main_module) ".c" )
#endif

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
     __Entry( meFile_Exit     , "&Quit" "\tAlt+F4"  , _Ctrl        , VK_Q , &File_Exit   ) \
   __EndSubMenu() \
   /*__SubMenu( "&Edit" ) \
      __Entry( meEdit_Undo    , "&Undo"  "\tCtrl+Z"  ,              ,      , &Edit_Undo ) \
      __Entry( meEdit_Redo    , "&Redo"              , _Ctrl+_Shift , VK_Z , &Edit_Redo ) \
      __Separator() \
      __Entry( meEdit_Find    , "&Find"              , _Ctrl        , VK_F , &Edit_Find ) \
      __Entry( meEdit_Replace , "Rep&lace"           , _Ctrl        , VK_H , &Edit_Replace ) \
      __Separator() \
      __Entry( meEdit_SelAll  , "&Select All"        , _Ctrl        , VK_A , &Edit_SelectAll  ) \
      __Separator() \
      __Entry( meEdit_Cut     , "C&ut"   "\tCtrl+X"  ,              ,      , &Edit_Cut  ) \
      __Entry( meEdit_Copy    , "&Copy"  "\tCtrl+C"  ,              ,      , &Edit_Copy ) \
      __Entry( meEdit_Paste   , "&Paste" "\tCtrl+V"  ,              ,      , &Edit_Paste) \
      __Separator() \
      __Entry( meCode_Build   , "&Build"             , 0            , VK_F6 , &Button_Compile ) \
      __Entry( meCode_Clear   , "Cl&ear output"      , _Ctrl+_Shift , VK_B , &Code_ClearOutput ) \
   __EndSubMenu()*/
//-------------------------------------------------------------------------------------------
   
#define _Shift FSHIFT
#define _Ctrl  FCONTROL
#define _Alt   FALT
#define Dummy()
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
        w->fMask      = MIIM_SUBMENU | MIIM_ID | MIIM_STRING;
        w->hSubMenu   = hResult ; w->wID = iID;
        w->dwTypeData = pzText;
    } _endwith
    InsertMenuItemA( hMenu , -1 , true , &tItem );
    if ( ( hMenu==g_WndMenu ) && (_CTL(wcMain)) ) { DrawMenuBar( _CTL(wcMain) ); }
    return hResult;
} // menu_AddSubMenu()
static int menu_MenuAddEntry( void* hMenu , int iID /* = 0 */ , char* pzText /* = NULL */ , void* pEvent /* = NULL */ , int bState /* = 0 */ ) {
    if (!IsMenu(hMenu)) { return -1; }
    MENUITEMINFOA tItem = { sizeof(MENUITEMINFO) };
    tItem.fMask      = MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_TYPE;
    tItem.fType      = ( pzText ? (( bState & MFT_RADIOCHECK ) ? MFT_RADIOCHECK : MFT_STRING ) : MFT_SEPARATOR );
    tItem.fState     = bState & (~MFT_RADIOCHECK);
    tItem.wID        = iID;
    tItem.dwItemData = (LONG_PTR)pEvent;
    if (pzText) { tItem.dwTypeData = pzText; } else { tItem.dwTypeData = NULL; }
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

static HMENU menu_CreateMainMenu(void) {   
    #define _SubMenu( _sText... ) \
    { \
      _auto hMenu = menu_AddSubMenu( hMenu , _sText , 0 ); \

    #define _EndSubMenu() }
    #define _Separator() menu_MenuAddEntry( hMenu , 0 , NULL , NULL , 0 );
    
    /* advanced macro for auto accelerator
    #if len(#_Accelerator)
        #if (_Modifiers and _Shift)
            #define _sShift "Shift+"
        #else
            #define _sShift
        #endif
        #if (_Modifiers and _Ctrl)
            #define _sCtrl "Ctrl+"
        #else
            #define _sCtrl
        #endif
        #if (_Modifiers and _Alt)
            #define _sAlt "Alt+"
        #else
            #define _sAlt
        #endif
        #if _Accelerator >= VK_F1 and _Accelerator <= VK_F24
            #define _sKey "F" & (_Accelerator-((VK_F1)-1))
        #elseif _Accelerator >= asc("A") and _Accelerator <= asc("Z")           
            #define _sKey +chr(_Accelerator)
        #elseif _Accelerator >= asc("0") and _Accelerator <= asc("9")           
            #define _sKey +chr(_Accelerator)
        #else
            #define _sKey s##_Accelerator
        #endif
        _const _sText2 = _Text "\t" _sCtrl _sAlt _sShift _sKey ;
        #undef _sCtrl
        #undef _sAlt
        #undef _sShift
        #undef _sKey
    #else
    */
        
    #define _Entry( _idName , _Text , _Modifiers , _Accelerator , _Callback... ) \
        { \
            _const _sText2 = _Text ; \
            menu_MenuAddEntry( hMenu , _idName , _sText2 , _Callback+0 , 0 ); \
        }
        
    _auto hMenu = CreateMenu() ; g_WndMenu = hMenu;      
      
    ForEachMenuEntry( _Entry ,  _SubMenu , _EndSubMenu , _Separator )

    return hMenu;
} //menu_CreateMainMenu()
