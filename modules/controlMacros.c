#include "controlFuncs.c"

if (_CTL(wcMain)) { return 0; }
_CTL(wcMain) = hwnd;

//just a macro to help creating controls
int iCurID, iResu;
#define _AddCtl( _ID , _ExStyle , _Class , _Style , _X , _Y , _Wid , _Hei , _Caption ) iCurID=_ID; g_CTL[iCurID] = (ControlStruct){ .tX = _X , .tY = _Y , .tW = _Wid , .tH = _Hei , .hwnd = CreateWindowEx(_ExStyle,_Class,_Caption,_Style,0,0,1,1024,hwnd,(HMENU)(iCurID),g_APPINSTANCE,NULL) };

_const UpDn = UPDOWN_CLASS;
_const cStyle = WS_CHILD;                         //Standard style for buttons class controls :)
_const cUpDnStyle = cStyle | UDS_AUTOBUDDY;       //' or UDS_SETBUDDYINT
_const cButtonStyle = cStyle;
_const cLabelStyle = cStyle;
_const cEdtStyle = cStyle | ES_AUTOHSCROLL;
_const cTxtStyle = cStyle | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | ES_MULTILINE;
_const cPanelStyle = cStyle | WS_VSCROLL | LBS_NOINTEGRALHEIGHT;
_const cCodeStyle = cStyle | WS_HSCROLL | WS_VSCROLL;
_const cComboStyle = cStyle | CBS_DROPDOWN | CBS_SORT;
_const cTreeStyle = cStyle | WS_VSCROLL | TVS_DISABLEDRAGDROP | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS;
_const cBrd = WS_EX_CLIENTEDGE;
_const cBsc = WS_EX_DLGMODALFRAME;
